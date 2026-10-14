#pragma once
#include <windows.h>

#include "core/config.hpp"

namespace uagfx::win {

// [display] mode (core/window_plan.hpp): leave the game's window as it is, make it borderless over the chosen
// monitor (the monitor's mode never changes), or a normal window of window_width x window_height (0 = the game's
// size), centred on that monitor.
void applyWindowMode(HWND hwnd, const Config& config, bool gameFullscreen, int gameWidth, int gameHeight);

// The game's window black until the renderer shows its first picture. In full screen the client makes its window
// 640x480 with a white background, maximises it and only then creates DirectDraw (registry.md section 2): while the
// renderer starts - its own process, DLSS - nothing repaints it, and the screen stayed black with the white 640x480
// area in its middle (0.10.2: "a white square the size of the login form"). Called at DirectDrawCreateEx, on the
// thread that made the window.
void blackenGameWindow();

}  // namespace uagfx::win
