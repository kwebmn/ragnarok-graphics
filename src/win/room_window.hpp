#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::roomwin {

// 3.0.11-beta.208 (the tester's third list: «alt c - вже створений чат - в старому стилі»): [interface] chat_rooms -
// the game's chat room window (UI window 0x1C at g_windowMgr + 0x104, class 0x697E10, SendMsg FUN_00460dd0) as a card
// (core/room_card): its title (+ 0x64), open (+ 0x80), count and places (+ 0x84, + 0x88); its lines and its members
// from its two list boxes (+ 0x98, + 0x9C: words + 0x58, colours + 0x94 - the owner's 0x808000); its edit (+ 0x94)
// shown as it is, a click giving it the keys (Enter sends as before); «Вийти» its close (6 / 0x87: the mode's 0x35);
// the owner's tools as its own menu does them: the settings (UI window 0x1E with the mode's + 0x394), «Вигнати» (the
// mode's 0x34 with the name), «Зробити власником» (0x33), «У друзі» (the manager's FUN_004cec50). The game's window
// moved off the screen (its edit keeps the keys); Esc does not leave the room. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::roomwin
