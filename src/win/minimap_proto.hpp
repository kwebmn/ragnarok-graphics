#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::minimapproto {

// 3.0.11-beta.1: [interface] minimap = 1 - the new interface's minimap prototype (stage 1, the owner's «давай»
// 2026-10-28; core/minimap_proto). Each frame on the game's thread: the game's own minimap hidden as Ctrl+Tab hides it
// (each time the game makes it again: a new map, Ctrl+Tab; back when switched off), the driver's panel in its corner
// with the map's picture round the player and the player's cell under it (to set against /where); the mouse over it
// kept from the game. The log tells what each step did.
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::minimapproto
