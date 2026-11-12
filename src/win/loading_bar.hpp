#pragma once

namespace uagfx::win::loadingbarwin {

// 3.0.18-beta.180: [interface] loading_bar (core/loading_bar) - while the game's map loading bar shows (its window 7)
// it is moved off the screen and the new one drawn at its place with the game's own per cent; off: put back. Each
// present (the loading's too), on the game's thread.
void frame(bool on, double k, int targetWidth, int targetHeight);
// Just before the game draws its windows (win/windows_draw - the loading's first frame too): its bar out of sight
void beforeGameDraw();

}  // namespace uagfx::win::loadingbarwin
