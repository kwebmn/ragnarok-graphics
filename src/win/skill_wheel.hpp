#pragma once
#include <windows.h>

namespace uagfx::win::skillwheelwin {

// 3.0.18-beta.132 (the pad analysis, variant A): a skill wheel open on the controller (pad/core Controller::wheelView)
// - eight of a hotbar row's cells (wheel 1 the second row's F1-F8, wheel 2 the third's) round the screen's middle,
// the one the right stick points at lit and named; drawn by uaUI, passive. `index` -1: none open. On the game's thread.
void frame(int index, int sector, double k, int targetWidth, int targetHeight);

}  // namespace uagfx::win::skillwheelwin
