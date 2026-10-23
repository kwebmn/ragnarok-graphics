#include "win/front_game.hpp"

namespace uagfx::win {
namespace {

// Whether a process runs: one Windows will not open for us (another user's, an elevated one) runs too
bool running(DWORD pid) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED;
    const bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
}

}  // namespace

FrontGame::~FrontGame() {
    if (front_) UnmapViewOfFile(front_);
    if (map_) CloseHandle(map_);
}

bool FrontGame::main(bool foreground) {
    if (!tried_) {
        tried_ = true;
        map_ = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(LONG),
                                  "Local\\uaro-graphics-front");
        if (map_) front_ = static_cast<LONG*>(MapViewOfFile(map_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LONG)));
    }
    if (!front_) return true;  // no shared word: every game the main one, as before
    const LONG me = LONG(GetCurrentProcessId());
    if (foreground) {
        if (InterlockedCompareExchange(front_, 0, 0) != me) InterlockedExchange(front_, me);
        return true;
    }
    const LONG front = InterlockedCompareExchange(front_, 0, 0);
    if (front == 0 || front == me) return true;
    const ULONGLONG now = GetTickCount64();
    if (front == checked_ && now - checkedAt_ < 1000) return false;
    checked_ = front;
    checkedAt_ = now;
    if (running(DWORD(front))) return false;
    InterlockedCompareExchange(front_, me, front);  // that game is gone: this one, unless another took its place first
    return InterlockedCompareExchange(front_, 0, 0) == me;
}

}  // namespace uagfx::win
