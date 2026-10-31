#pragma once
#include <cstdint>

namespace uagfx::stallwatch {

// 3.0.11-beta.94 (R-3S33MK «чтото лагает игра»: the game's thread stood still 15 s and 5 s - Windows put its «not
// responding» ghost over the window - and the log did not say where): the game's thread watched by its message loop's
// beats (each PeekMessageA of uaRO.exe, win/stall_watch) from the profiler's thread. A second without a beat begins a
// stall (the thread sampled from then, win/profiler); the next beat ends it, its length from the last beat seen.
constexpr uint64_t kStillMs = 1000;

enum class Event { None, Began, Ended };

class Watch {
public:
    // The beats counted so far, now (ms)
    Event look(uint32_t beats, uint64_t nowMs);
    bool still() const { return still_; }
    uint64_t stillMs() const { return stillMs_; }  // the last stall's length, at its end

private:
    bool seen_ = false, still_ = false;
    uint32_t beats_ = 0;
    uint64_t changedMs_ = 0, stillMs_ = 0;
};

}  // namespace uagfx::stallwatch
