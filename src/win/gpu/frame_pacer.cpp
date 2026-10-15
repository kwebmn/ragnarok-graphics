#include "win/gpu/frame_pacer.hpp"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace uagfx::gpu {

FramePacer::~FramePacer() {
    if (timer_) CloseHandle(timer_);
}

void FramePacer::wait(int fps) {
    if (fps <= 0) {
        next_ = 0;
        return;
    }
    LARGE_INTEGER now;
    if (!frequency_) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        frequency_ = f.QuadPart;
    }
    if (!tried_) {  // Windows 10 1803 and later; else an ordinary one (about a millisecond or the system's tick)
        tried_ = true;
        timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!timer_) timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }
    const LONGLONG period = frequency_ / fps;
    QueryPerformanceCounter(&now);
    if (!next_ || now.QuadPart - next_ > period) {  // the first frame, or one late: the schedule from now
        next_ = now.QuadPart + period;
        return;
    }
    const LONGLONG left = next_ - now.QuadPart;
    const LONGLONG halfMs = frequency_ / 2000;
    if (left > halfMs && timer_) {
        LARGE_INTEGER due;
        due.QuadPart = -((left - halfMs) * 10000000 / frequency_);  // 100 ns units, relative
        if (SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer_, 1000);
    } else if (left > halfMs) {
        Sleep(DWORD((left - halfMs) * 1000 / frequency_));
    }
    do {  // the rest of it
        QueryPerformanceCounter(&now);
        if (next_ - now.QuadPart > halfMs / 4) Sleep(0);
    } while (now.QuadPart < next_);
    next_ += period;
}

}  // namespace uagfx::gpu
