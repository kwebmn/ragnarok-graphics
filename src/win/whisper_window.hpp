#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::whisperwin {

// 3.0.11-beta.212 (the tester: «віспер чат - в старому стилі»): [interface] whispers - the game's 1:1 whisper windows
// (class 0x699BC8, SendMsg FUN_004869e0; the manager makes one a partner as their words come, FUN_004db310) as cards
// (core/whisper_card), cascaded: the partner (+ 0x64), the lines (the list box + 0x88: words + 0x58, colours + 0x94),
// the edit (+ 0x84) shown as it is - a click gives it the keys (Enter sends as before) -, «Надіслати» its send (6 /
// 0x76: a whisper to the partner, a «/» command the game's own parser), ✕ its close (6 / 0x87). The game's windows
// moved off the screen (their edits keep the keys; one opened at the place this left put back where it shows when
// the cards are off). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: the top one's close
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::whisperwin
