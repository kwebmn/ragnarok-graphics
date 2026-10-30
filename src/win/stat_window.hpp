#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::statswin {

// 3.0.11-beta.42 (stage 8 of the new interface, its windows; the owner: «делай окна»): [interface] status -
// «Характеристики» (Alt+A) redrawn (core/stat_window) while the game's own is open: the game's window (UI window 0xB,
// g_windowMgr + 0x80, its class's vtable 0x698700) stays made, its shown flag held at 0; Alt+A and the character
// block's button open and close it as before. The numbers the session's (core/char_status); a «+» asks as its own
// does (FUN_004722a0: the game mode's message 0x20 with the stat, 13 STR .. 18 LUK, and 1). Its place kept in
// uaro-graphics\stats-state.txt. On the game's thread.
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::statswin
