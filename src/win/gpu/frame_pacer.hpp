#pragma once
#include <windows.h>

namespace uagfx::gpu {

// 0.85: [frames] fps_limit kept to the frame: Sleep() wakes up to 16 ms late (60 frames became 32-64, unevenly). The
// presents are kept on a schedule of 1/limit seconds - slept on a high-resolution timer to just before, then the last
// half a millisecond waited out; a frame late (a load, a heavy one) starts the schedule anew instead of rushing after.
class FramePacer {
public:
    ~FramePacer();
    // before a present; 0: no limit. 2.5.20: sleep - on the timer to the frame's time (a laptop on its battery): no
    // last half a millisecond waited out awake
    void wait(int fps, bool sleep = false);

private:
    HANDLE timer_ = nullptr;
    bool tried_ = false;
    LONGLONG next_ = 0;
    LONGLONG frequency_ = 0;
};

}  // namespace uagfx::gpu
