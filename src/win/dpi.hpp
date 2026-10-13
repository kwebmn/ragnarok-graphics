#pragma once
#include <windows.h>

namespace uagfx::win {

// The window's client area in the screen's real pixels (physical), or the size Windows tells this game. uaRO.exe is
// not DPI-aware: under Windows scaling it is told a smaller size and Windows stretches its window, unless the frame
// is shown in real pixels (stage 3 spec §2: measured pixel for pixel on the game's own window).
SIZE clientPixels(HWND hwnd, bool physical);

// While it lives, this thread is per-monitor DPI aware: what it measures and places is in the screen's real pixels.
// Windows 10 1607 and newer; before that active() is false and nothing changes.
class RealPixels {
public:
    RealPixels();
    ~RealPixels();
    RealPixels(const RealPixels&) = delete;
    RealPixels& operator=(const RealPixels&) = delete;
    bool active() const { return old_ != nullptr; }

private:
    DPI_AWARENESS_CONTEXT old_ = nullptr;
};

}  // namespace uagfx::win
