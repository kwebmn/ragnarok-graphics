#include "win/notifier.hpp"

#include <shellapi.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <variant>

#include "core/notify.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/packet_tap.hpp"
#include "win/real_ddraw.hpp"
#include "win/window_switch.hpp"

namespace uagfx::win::notifier {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C;  // CSession's account id (the 0x196 handler compares with it)
constexpr uint32_t kSession = 0x771148, kIgnored = 0x639F60;  // CSession::IsIgnored(name) - the whisper's own check
const uint8_t kIgnoredCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x8B, 0x81, 0xF8, 0x18, 0x00, 0x00};
constexpr UINT kIconId = 1;

using IgnoredFn = int(__attribute__((thiscall)) *)(void* session, const char* name);
using NotifyFn = BOOL(WINAPI*)(DWORD message, NOTIFYICONDATAW* data);

struct State {
    HWND window = nullptr;
    bool front = true, logged = false, icon = false;
    NotifySettings settings;
    unsigned codePage = CP_ACP;
    notify::Gate gate;
    NotifyFn shell = nullptr;
    bool shellTried = false;
};

State& state() {
    static State s;
    return s;
}

std::string fromGame(const std::string& text) { return utf8Of(widened(text, state().codePage)); }

void copy(wchar_t* to, size_t room, const std::wstring& from) {
    const size_t n = from.size() < room - 1 ? from.size() : room - 1;
    std::memcpy(to, from.data(), n * sizeof(wchar_t));
    to[n] = L'\0';
}

bool ignored(const std::string& name) {
    static const bool known =
        std::memcmp(reinterpret_cast<const void*>(uintptr_t(kIgnored)), kIgnoredCode, sizeof kIgnoredCode) == 0;
    if (!known) return false;
    char padded[32] = {};
    std::memcpy(padded, name.data(), name.size() < 24 ? name.size() : 24);
    const auto fn = reinterpret_cast<IgnoredFn>(uintptr_t(kIgnored));
    return fn(reinterpret_cast<void*>(uintptr_t(kSession)), padded) != 0;
}

// The balloon: the character's name above, the words below; the game's own icon
void show(State& s, const std::string& words) {
    if (!s.shellTried) {
        s.shellTried = true;
        const HMODULE shell = LoadLibraryA("shell32.dll");
        const FARPROC fn = shell ? GetProcAddress(shell, "Shell_NotifyIconW") : nullptr;
        s.shell = reinterpret_cast<NotifyFn>(reinterpret_cast<void*>(fn));
    }
    if (!s.shell || !s.window) return;
    NOTIFYICONDATAW d{};
    d.cbSize = sizeof d;
    d.hWnd = s.window;
    d.uID = kIconId;
    d.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_INFO;
    d.uCallbackMessage = kMessage;
    HICON icon = reinterpret_cast<HICON>(GetClassLongPtrW(s.window, GCLP_HICON));
    d.hIcon = icon ? icon : LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    const std::string name = readOwnName();
    const std::wstring title = widened(name.empty() ? std::string("uaRO") : fromGame(name), CP_UTF8);
    copy(d.szTip, sizeof d.szTip / sizeof d.szTip[0], title);
    copy(d.szInfoTitle, sizeof d.szInfoTitle / sizeof d.szInfoTitle[0], title);
    copy(d.szInfo, sizeof d.szInfo / sizeof d.szInfo[0], widened(words, CP_UTF8));
    d.dwInfoFlags = NIIF_INFO;
    s.icon = s.shell(s.icon ? NIM_MODIFY : NIM_ADD, &d) != FALSE || s.icon;
}

void hide(State& s) {
    if (!s.icon || !s.shell || !s.window) return;
    NOTIFYICONDATAW d{};
    d.cbSize = sizeof d;
    d.hWnd = s.window;
    d.uID = kIconId;
    s.shell(NIM_DELETE, &d);
    s.icon = false;
}

void tell(notify::Kind kind, const std::string& who, const std::string& what) {
    State& s = state();
    if (!s.window || !notify::wanted(s.settings, kind, s.front)) return;
    FLASHWINFO f{sizeof f, s.window, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0};  // until it is in front again
    FlashWindowEx(&f);
    if (!s.gate.pass(kind, GetTickCount64())) return;  // a burst: the flashing says it
    show(s, notify::text(startup().lang, kind, fromGame(who), fromGame(what)));
    LOG_INFO("notify: %d told from behind", int(kind));
}

}  // namespace

void frame(HWND window, bool front, const Config& config) {
    State& s = state();
    if (!s.logged) {
        s.logged = true;
        LOG_INFO("notify: %s", packetTapOn() ? "the game's packets read - whispers, invitations and deaths told from "
                                               "behind ([notify])"
                                             : "not this uaRO.exe's packet loop - no notifications from the game");
    }
    if (front && !s.front) hide(s);  // back: the player sees the game itself
    s.window = window;
    s.front = front;
    s.settings = config.notify;
    s.codePage = gameCodePage(config);
}

void packet(const packets::Event& e) {
    const auto own = [](uint32_t id) {
        return knownGameBuild() && id == *reinterpret_cast<const uint32_t*>(uintptr_t(kOwnAccount));
    };
    if (const auto* w = std::get_if<packets::Whisper>(&e)) {
        if (ignored(w->from)) return;
        windowmarks::whispered();  // 3.0.4-beta.4: on its card in the other windows' panels
        tell(notify::Kind::Whisper, w->from, w->text);
    } else if (const auto* h = std::get_if<packets::Hit>(&e)) {
        if (own(h->target)) windowmarks::hit();
    } else if (const auto* p = std::get_if<packets::PartyInvite>(&e)) {
        tell(notify::Kind::Party, p->party, "");
    } else if (const auto* t = std::get_if<packets::TradeRequest>(&e)) {
        tell(notify::Kind::Trade, t->from, "");
    } else if (const auto* g = std::get_if<packets::GuildInvite>(&e)) {
        tell(notify::Kind::Guild, g->guild, "");
    } else if (const auto* f = std::get_if<packets::FriendRequest>(&e)) {
        tell(notify::Kind::Friend, f->from, "");
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        if (v->type == 1 && own(v->id)) {
            windowmarks::died();
            tell(notify::Kind::Death, "", "");
        }
    }
}

bool message(HWND window, UINT m, WPARAM, LPARAM lp) {
    if (m != kMessage) return false;
    const UINT what = LOWORD(lp);
    if (what == NIN_BALLOONUSERCLICK || what == WM_LBUTTONUP) {  // the notification or the icon clicked
        if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
    }
    return true;
}

void gone(HWND window) {
    State& s = state();
    if (s.window == window) hide(s);
}

}  // namespace uagfx::win::notifier
