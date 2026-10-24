#include "win/game_windows.hpp"

#include <cstring>

namespace uagfx::win {
namespace {

// Its game runs and the window is still its own: a game's windows go with its process, a handle used again is
// another process's
bool alive(const gamewindows::Entry& e) {
    const HWND w = reinterpret_cast<HWND>(uintptr_t(e.window));
    DWORD pid = 0;
    return w && IsWindow(w) && GetWindowThreadProcessId(w, &pid) && pid == e.pid;
}

LONG seqOf(gamewindows::Entry& e) { return InterlockedCompareExchange(reinterpret_cast<LONG*>(&e.seq), 0, 0); }

}  // namespace

SharedWindows::~SharedWindows() {
    if (table_) UnmapViewOfFile(table_);
    if (map_) CloseHandle(map_);
}

bool SharedWindows::open() {
    if (!tried_) {
        tried_ = true;
        map_ = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(gamewindows::Table),
                                  "Local\\uaro-graphics-windows-2");  // 3.0.3-beta.9's layout
        if (map_)
            table_ = static_cast<gamewindows::Table*>(
                MapViewOfFile(map_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(gamewindows::Table)));
    }
    return table_ != nullptr;
}

void SharedWindows::publish(const gamewindows::Entry& mine) {
    if (!open() || mine.number < 1 || mine.number > uint32_t(gamewindows::kMost)) return;
    gamewindows::Entry& e = table_->entries[mine.number - 1];
    auto* seq = reinterpret_cast<LONG*>(&e.seq);
    const LONG odd = seqOf(e) | 1;  // odd whatever a game gone in the middle of a write left
    InterlockedExchange(seq, odd);
    std::memcpy(reinterpret_cast<char*>(&e) + sizeof e.seq, reinterpret_cast<const char*>(&mine) + sizeof e.seq,
                sizeof e - sizeof e.seq);
    InterlockedExchange(seq, odd + 1);  // even: whole
}

void SharedWindows::leave(int number) {
    if (!table_ || number < 1 || number > gamewindows::kMost) return;
    gamewindows::Entry& e = table_->entries[number - 1];
    if (e.pid != GetCurrentProcessId()) return;
    gamewindows::Entry none{};
    none.number = uint32_t(number);
    publish(none);
}

int SharedWindows::claim() {
    if (!open()) return 0;
    const DWORD self = GetCurrentProcessId();
    for (int i = 0; i < gamewindows::kMost; ++i)
        if (table_->entries[i].pid == self) return i + 1;
    for (int i = 0; i < gamewindows::kMost; ++i) {
        const gamewindows::Entry& e = table_->entries[i];
        if (e.pid == 0 || !alive(e)) return i + 1;
    }
    return 0;
}

int32_t SharedWindows::summon(uint32_t pid) {
    if (!open()) return 0;
    InterlockedExchange(reinterpret_cast<LONG*>(&table_->summonPid), LONG(pid));
    return int32_t(InterlockedIncrement(reinterpret_cast<LONG*>(&table_->summonSerial)));
}

bool SharedWindows::summonFor(uint32_t self, int32_t& seen, int32_t& serial) {
    if (!open()) return false;
    serial = int32_t(InterlockedCompareExchange(reinterpret_cast<LONG*>(&table_->summonSerial), 0, 0));
    const int32_t pid = int32_t(InterlockedCompareExchange(reinterpret_cast<LONG*>(&table_->summonPid), 0, 0));
    return gamewindows::summoned(pid, serial, self, seen);
}

uint32_t SharedWindows::readyOf(int number) {
    if (!open() || number < 1 || number > gamewindows::kMost) return 0;
    gamewindows::Entry& e = table_->entries[number - 1];
    for (int tries = 0; tries < 4; ++tries) {
        const LONG before = seqOf(e);
        if (before & 1) continue;
        const uint32_t ready = e.ready;
        MemoryBarrier();
        if (seqOf(e) == before) return ready;
    }
    return 0;
}

std::vector<gamewindows::Window> SharedWindows::live() {
    gamewindows::Table copy{};
    if (!open()) return {};
    for (int i = 0; i < gamewindows::kMost; ++i) {
        gamewindows::Entry& e = table_->entries[i];
        for (int tries = 0; tries < 4; ++tries) {
            const LONG before = seqOf(e);
            if (before & 1) continue;  // being written: once more
            std::memcpy(&copy.entries[i], &e, sizeof e);
            MemoryBarrier();
            if (seqOf(e) == before) break;
            copy.entries[i] = gamewindows::Entry{};  // written meanwhile: once more, or none
        }
    }
    return gamewindows::windows(copy, alive);
}

HANDLE ownWake() {
    static HANDLE event = CreateEventA(nullptr, FALSE, FALSE, gamewindows::wakeName(GetCurrentProcessId()).c_str());
    return event;
}

void wake(uint32_t pid) {
    if (HANDLE e = OpenEventA(EVENT_MODIFY_STATE, FALSE, gamewindows::wakeName(pid).c_str())) {
        SetEvent(e);
        CloseHandle(e);
    }
}

bool switchTo(const gamewindows::Window& w) {
    const HWND h = reinterpret_cast<HWND>(uintptr_t(w.window));
    if (!IsWindow(h)) return false;
    if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
    AllowSetForegroundWindow(w.pid);
    return SetForegroundWindow(h) != FALSE;
}

}  // namespace uagfx::win
