#pragma once
#include <windows.h>

#include "core/config.hpp"

namespace uagfx::win {

// [display] mode: borderless over the chosen monitor (the monitor's mode never changes), or a normal window of
// window_width x window_height (0 = the game's size), centred on that monitor.
void applyWindowMode(HWND hwnd, const Config& config, int gameWidth, int gameHeight);

}  // namespace uagfx::win
