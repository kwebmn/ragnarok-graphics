#pragma once
#include <windows.h>

#include "core/config.hpp"
#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::charpanel {

// 3.0.11-beta.36 (stage 7 of the new interface; the owner: «кольца или полоски на выбор. приступай»): [interface]
// character = bars | rings - the character's block at the top left (core/char_block) in place of the game's Basic
// Info window. The game keeps that window (UI window 0, g_windowMgr + 0x90, its class's vtable 0x6984A0): only its
// shown flag (+0x38, which the windows' manager reads to draw it and to give it the mouse) is held at 0 while the block
// shows, and its buttons stay its own - the block's send the window the same commands (its SendMsg, vtable + 0x94:
// message 6, 0x7E the stats, 0x81 the equipment ...). The numbers from the game's session (core/char_status), the
// portrait from its head's sprite and hair palette. On the game's thread.
void frame(HWND game, CharacterLook look, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.11-beta.39: its block's box in the target while it shows (the party's panel stands under it); false: not shown
bool panelBox(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::charpanel
