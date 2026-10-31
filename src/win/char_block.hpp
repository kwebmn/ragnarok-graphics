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
// 3.0.11-beta.53: just before the game draws its windows (win/windows_draw) - its Basic Info hidden, never a frame
void beforeGameDraw();

// 3.0.11-beta.90 (the controller's windows' tile): one of the game's Basic Info window's buttons pressed as its own
// (its SendMsg message 6 with the command: 0x7E stats, 0x81 equip, 0x80 items, 0x82 skills, 0x85 friends, 0x7F
// options) - the game's window opened or closed; false: no such window now
bool pressGame(int command);
// 3.0.11-beta.93 (the controller's B closes them): the game's window that button opens is open now - the windows'
// manager keeps a pointer to each open one (its DeleteWindow FUN_004d1e60): 0x81 the equipment (window 10, + 0x94),
// 0x82 the skills (0x25, + 0x118), 0x7F the options (0xD, + 0x98), 0x85 the party and friends (0x22, + 0x11C)
bool gameOpen(int command);

}  // namespace uagfx::win::charpanel
