#include "win/map_entry_hook.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include "core/login_exe.hpp"
#include "core/map_entry.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/strings.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/text_window.hpp"

namespace uagfx::win {
namespace {

using namespace login_exe;

struct Fds {  // as Winsock's fd_set - ddraw.dll takes select() from the game's ws2_32.dll, imports none of it
    unsigned count;
    uintptr_t sockets[64];
};
struct Wait {  // timeval
    long seconds, micro;
};
using SelectFn = int(WINAPI*)(int, Fds*, Fds*, Fds*, const Wait*);
using QueueFn = int(__attribute__((thiscall))*)(void*, int, const uint8_t*);
using CloseFn = void(__attribute__((thiscall))*)(void*);

int g_ready = -1;  // once: 1 this build of uaRO.exe, 0 another
bool g_patientNow = false;
bool g_pause = true;
map_entry::Guard g_guard;
struct Held {  // a character's selection held back (CH_SELECT_CHAR)
    bool on = false;
    uint8_t bytes[16] = {};
    int len = 0;
    uint64_t nextPing = 0;
    settings::Lang lang = settings::Lang::En;
} g_held;
HWND g_notice = nullptr;
std::wstring g_noticeText;
bool g_loaded = false;    // the file's hold taken
uint64_t g_savedWall = 0;  // the file's: no selection before this (seconds of the wall clock, 0 none)
uint64_t g_lastHold = 0;   // the guard's hold as last written

bool codeIs(uint32_t at, const uint8_t* bytes, size_t size) {
    uint8_t now[16] = {};
    SIZE_T got = 0;
    return size <= sizeof now &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), now, size, &got) &&
           got == size && std::memcmp(now, bytes, size) == 0;
}

bool write(uint32_t at, const void* bytes, size_t size) {
    auto* p = reinterpret_cast<uint8_t*>(uintptr_t(at));
    DWORD old = 0, unused = 0;
    if (!VirtualProtect(p, size, PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(p, bytes, size);
    VirtualProtect(p, size, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), p, size);
    return true;
}

void* connection() {
    return (*reinterpret_cast<const uint8_t*>(uintptr_t(kConnectionBuilt)) & 1) ? reinterpret_cast<void*>(kConnection)
                                                                                  : nullptr;
}

int queue(void* conn, int len, const uint8_t* data) { return reinterpret_cast<QueueFn>(kQueue)(conn, len, data); }

uint64_t nowMs() { return GetTickCount64(); }

uint64_t wallSeconds() {
    FILETIME f{};
    GetSystemTimeAsFileTime(&f);
    return ((uint64_t(f.dwHighDateTime) << 32) | f.dwLowDateTime) / 10000000ull;
}

std::string holdFile() { return gameDir() + "\\uaro-graphics\\map-entry.txt"; }

void saveHold(uint64_t wall) {
    if (wall == g_savedWall) return;
    if (writeFileAtomic(holdFile(), std::to_string(wall) + "\r\n")) g_savedWall = wall;
}

// Once: the hold a former client left (a failed entry's, or the one after its exit), in the guard's clock
void loadHold(uint64_t now) {
    g_loaded = true;
    std::string text;
    if (!readTextFile(holdFile(), text)) return;
    const uint64_t until = std::strtoull(text.c_str(), nullptr, 10), wall = wallSeconds();
    g_savedWall = until;
    if (until <= wall) return;
    g_guard.holdAtLeastUntil(now + (until - wall) * 1000);
    g_lastHold = g_guard.holdUntil();
    LOG_INFO("map entry: the former client's hold - the first selection waits up to %u s",
             unsigned(until - wall));
}

// The client's queueing of CH_SELECT_CHAR (its call in CLoginMode state 9, kSelectCall): held back while the map
// server still has a failed entry's node, else as the game would.
int __attribute__((thiscall)) selectQueued(void* conn, int len, const uint8_t* data) {
    const uint64_t now = nowMs();
    if (g_pause && len >= 3 && len <= int(sizeof g_held.bytes) && data[0] == 0x66 && data[1] == 0 &&
        g_guard.holding(now, true)) {
        std::memcpy(g_held.bytes, data, size_t(len));
        g_held.len = len;
        g_held.on = true;
        g_held.nextPing = now + 10000;
        std::string text;
        readTextFile(gameDir() + "\\uaro-graphics.ini", text);
        const auto ini = settings::readIni(text);
        const auto word = ini.find("utility.language");
        const auto chosen = word != ini.end() ? settings::parseLang(word->second) : std::nullopt;
        g_held.lang = settings::startLang(chosen);
        LOG_INFO("map entry: the character's selection waits %u s - the map server still has the failed entry",
                 unsigned((g_guard.holdUntil() - now + 999) / 1000));
        return 1;
    }
    return queue(conn, len, data);
}

bool ready() {
    if (g_ready >= 0) return g_ready == 1;
    g_ready = codeIs(kVtableStoreAt, kVtableStoreBytes, sizeof kVtableStoreBytes) &&
              codeIs(kChangeState, kChangeStateBytes, sizeof kChangeStateBytes) &&
              codeIs(kClose, kCloseBytes, sizeof kCloseBytes) && codeIs(kQueue, kQueueBytes, sizeof kQueueBytes) &&
              codeIs(kWaitAt, kWaitBytes, sizeof kWaitBytes) && codeIs(kSelectAt, kSelectBytes, sizeof kSelectBytes);
    if (g_ready) {
        const int32_t rel = int32_t(reinterpret_cast<uintptr_t>(&selectQueued) - (uintptr_t(kSelectCall) + 5));
        if (!write(kSelectCall + 1, &rel, sizeof rel)) g_ready = 0;
    }
    LOG_INFO(g_ready ? "map entry: uaRO.exe's way onto a map watched ([connection])"
                     : "map entry: the game's own way (not where this build of uaRO.exe has it)");
    return g_ready == 1;
}

void setPatient(bool on) {
    if (on == g_patientNow) return;
    if (write(kWaitImm, on ? kPatientImm : kWaitBytes + 8, 4)) g_patientNow = on;
    LOG_INFO("map entry: the client waits for the map server %u s",
             on ? unsigned(map_entry::kPatientWaitMs / 1000) : 15u);
}

// The map's connection this frame: made (writable) or failed (an exception - a connect refused or timed out)
void probe(void* conn, map_entry::Seen& s) {
    static const auto select =
        reinterpret_cast<SelectFn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ws2_32.dll"), "select")));
    const uintptr_t sock = *reinterpret_cast<const uintptr_t*>(static_cast<uint8_t*>(conn) + 4);
    if (!select || sock == ~uintptr_t(0)) return;
    Fds writable{1, {sock}}, failed{1, {sock}};
    const Wait none{0, 0};
    if (select(0, nullptr, &writable, &failed, &none) <= 0) return;
    s.connected = writable.count > 0;
    s.failed = failed.count > 0;
}

void showNotice(HWND game, const std::wstring& text) {
    if (text == g_noticeText && g_notice && IsWindowVisible(g_notice)) return;
    g_noticeText = text;
    if (game && IsWindowVisible(game) && GetForegroundWindow() == game)
        drawTextWindow(g_notice, L"uaroGraphicsNotice", game, text, TextPlace::TopMiddle);
    else if (g_notice)
        ShowWindow(g_notice, SW_HIDE);
}

void hideNotice() {
    g_noticeText.clear();
    if (g_notice) ShowWindow(g_notice, SW_HIDE);
}

std::wstring wide(const std::string& s) {
    std::wstring w(size_t(MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), nullptr, 0)), L'\0');
    if (!w.empty()) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), w.data(), int(w.size()));
    return w;
}

}  // namespace

void mapEntryAtExit() {
    if (g_ready != 1) return;
    const uint64_t wall = wallSeconds(), after = wall + map_entry::kAfterExitMs / 1000;
    saveHold(g_savedWall > after ? g_savedWall : after);
}

void mapEntryFrame(const ConnectionSettings& settings, HWND game) {
    if (!ready()) return;
    setPatient(settings.patientEntry);
    g_pause = settings.entryPause;
    const uint64_t now = nowMs();
    if (!g_loaded) loadHold(now);
    uint8_t* mode = *reinterpret_cast<uint8_t* const*>(uintptr_t(kModeNow));
    map_entry::Seen s;
    s.loginMode = mode && *reinterpret_cast<const uint32_t*>(mode) == kLoginVtable;
    if (s.loginMode) s.state = *reinterpret_cast<const int*>(mode + kStateAt);
    void* conn = connection();
    if (s.loginMode && s.state == kStateEntering && conn) probe(conn, s);
    if (g_guard.frame(s, now, settings.patientEntry) == map_entry::Step::Reconnect && conn) {
        LOG_INFO("map entry: the connection to the map server did not come up - connecting again");
        reinterpret_cast<CloseFn>(kClose)(conn);
        *reinterpret_cast<int*>(mode + kNextStateAt) = kStateEntering;  // the game connects again in its own turn
    }
    if (g_guard.holdUntil() != g_lastHold) {  // a failed entry's hold set, or one done with: the file follows
        g_lastHold = g_guard.holdUntil();
        saveHold(g_lastHold > now ? wallSeconds() + (g_lastHold - now + 999) / 1000 : 0);
    }
    if (!g_held.on) return;
    if (!s.loginMode || s.state != kStateSelecting || !conn) {  // the player went back, or the char server is gone
        g_held.on = false;
        hideNotice();
        LOG_INFO("map entry: the held selection dropped (the character screen left)");
        return;
    }
    if (!g_guard.holding(now, settings.entryPause)) {
        g_held.on = false;
        hideNotice();
        queue(conn, g_held.len, g_held.bytes);
        LOG_INFO("map entry: the character's selection sent");
        return;
    }
    if (now >= g_held.nextPing) {  // the char server closes a connection quiet for a minute: its ping, as the game's
        uint8_t ping[6] = {0x87, 0x01};
        std::memcpy(ping + 2, reinterpret_cast<const void*>(uintptr_t(kAccount)), 4);
        queue(conn, sizeof ping, ping);
        g_held.nextPing = now + 10000;
    }
    const unsigned left = unsigned((g_guard.holdUntil() - now + 999) / 1000);
    char clock[16];
    std::snprintf(clock, sizeof clock, "%u:%02u", left / 60, left % 60);
    showNotice(game, wide(settings::fmt(settings::tr(g_held.lang, settings::Text::ConnEntryWait), {clock})));
}

}  // namespace uagfx::win
