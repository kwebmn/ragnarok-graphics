#pragma once
#include "core/config.hpp"

// 3.0.22-beta.6 (the owner's «ок» to idea 10 - «убрать рывки 72/48 кадров на 4K при 175 %»). The facts: uaRO.exe is
// not DPI aware, so its window's client is whole points - 3840 / 1.75 = 2194 points come back as 3839 pixels, short of
// the monitor (R-PSFZ8K), and DWM composes such a window: at 144 Hz its frames land 13.9 or 20.8 ms apart. Made
// per-monitor aware at its creation (the 3.0.11-beta.292 trial, which the owner played with until 3.0.21-beta.16) it
// covered the monitor - «the window covers its monitor (3840x2160)», 100-140 FPS (R-P2NGJV). Here: where it is needed.
namespace uagfx::dpiaware {

// Windows scales the monitor (dpi above 96) and its pixels are not a whole number of points across or down - a
// DPI-unaware window's client of whole points does not land on them
bool inexact(int width, int height, unsigned dpi);
// The game's window made per-monitor aware: on, or auto in full screen (`fullScreen` 1; 0 a window - Windows' own
// size kept; -1 not known) on such a monitor
bool wanted(DpiAware mode, int fullScreen, int width, int height, unsigned dpi);

}  // namespace uagfx::dpiaware
