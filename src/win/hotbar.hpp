#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::hotbarwin {

// 3.0.11-beta.40 (stage 8 of the new interface, the hotbar first; the owner: «делай дальше»): [interface] hotbar - the
// game's hotkeys bar redrawn at the bottom middle (core/hotbar_panel). The game keeps its bar (UI window 0x24,
// g_windowMgr + 0x9C, its class's vtable 0x6985D0): its shown flag (+0x38) held at 0 while this one shows, its keys
// (F1-F9, the battle mode's letters - they send the window its message 0x29 hidden or not) its own. What is in each
// slot from the game's session (+0x314 a slot's words, +0x4C4 a skill's level), an item's count from the bag; a click
// sends the window the same message 0x29 the keys do. The pencil shows the game's own bar to drag skills and items
// into it. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// A skill or an item dragged from the game's windows and let go at the game's (x, y) over one of its cells (`drag`: the
// game mode's drag, + 0x32C): the game's own bar takes it as dropped on that cell (its drop, vtable + 0x2C - the slot
// set, the drag ended; the battle mode's row the cell's); false: not over a cell (win/world_mouse asks)
bool drop(int x, int y, uint32_t drag);

}  // namespace uagfx::win::hotbarwin
