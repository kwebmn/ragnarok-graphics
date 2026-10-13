#pragma once
#include <windows.h>

namespace uagfx::win {

// The window's client area in the screen's real pixels (physical), or the size Windows tells this game. uaRO.exe is
// not DPI-aware: under Windows scaling it is told a smaller size and Windows stretches its window, unless the frame
// is shown in real pixels (stage 3 spec §2: measured pixel for pixel on the game's own window).
SIZE clientPixels(HWND hwnd, bool physical);

}  // namespace uagfx::win
