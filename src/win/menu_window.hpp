#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::menuwin {

// 3.0.11-beta.140 (the owner «давай» to the first of the game's windows not redrawn yet): [interface] menu - the game's
// menu and its death screen redrawn (core/menu_window): Esc while alive opens UI window 0x30 (g_windowMgr + 0xD8, its
// class's vtable 0x699248, SendMsg FUN_0047d550), the death 0x13 (+0xDC, 0x6992E0, FUN_0047d9f0) - or 0x88 with a Token
// of Siegfried (item 7621) in the bag (+0x218, 0x69A8E8, FUN_0049b990). The game's window stays made, its shown flag
// held at 0; our buttons press its own (message 6 with the button's id) - the game does what its own did (the game
// mode's 0x1B restart: 0 the save point, 1 character select; 0x65 the exit; 0x10A the token's revival) and deletes its
// window. Esc stays the game's (it closes its window, or opens it). On the game's thread.
void setRecap(bool on);  // 3.0.11-beta.277: [interface] death_recap - the last hits on the death screen
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);  // ↑ ↓ choose, Enter presses - while it shows
bool shown();
void beforeGameDraw();  // its game's window hidden
// Played with the controller: the D-pad chooses, A presses, B «Повернутися до гри»
enum class PadCommand : uint8_t { Up, Down, Press, Back };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::menuwin
