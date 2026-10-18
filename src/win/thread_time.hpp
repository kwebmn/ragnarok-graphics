#pragma once
#include <windows.h>

namespace uagfx::win {

// 2.5.15: the processor's time this thread has had (kernel and user), in ms - its work, not its waits: the log's
// "busy" of the game's thread and of Direct3D 11's own (an HD 6570's player: was the processor the limit?)
inline double threadCpuMs() {
    FILETIME created, exited, kernel, user;
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return 0.0;
    const auto ticks = [](const FILETIME& t) { return (ULONGLONG(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
    return double(ticks(kernel) + ticks(user)) / 10000.0;  // 100 ns units
}

}  // namespace uagfx::win
