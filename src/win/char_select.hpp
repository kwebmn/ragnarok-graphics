#pragma once
#include <windows.h>

#include <cstdint>

#include "core/accounts.hpp"
#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::charselectwin {

// 3.0.11-beta.152 (task 3 of the remaining windows, the login's part two): [interface] char_select - the game's
// character select (UI window 5 at g_windowMgr + 0x20C, its class 0x697BA0, SendMsg FUN_0045ae20: the place chosen
// + 0x8F0, the page + 0x8F4, the pages + 0x8F8, the three places' controls + 0x918) in the new look
// (core/char_select): each character as the game mode gives it (its message 8 with the place's number: the 108-byte
// CHARACTER_INFO - its name + 0x4A), its face (win/head_picture); a click chooses a place as a click on the game's
// own (message 6, 0x8A from that place's control), two clicks enter; the buttons press its own (0x76 enter, 0x89
// make, 0x91 delete, 0x77 back, 0x96 / 0x97 the pages); the game's window off the screen. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
void beforeGameDraw();
enum class PadCommand : uint8_t { Left, Right, Enter, Back, PageBefore, PageAfter, Make };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);
// 3.0.11-beta.175 (the login concept's shelf): the game's character select there (whatever [interface] char_select
// says) - the login was accepted; and the character chosen in it when it was last seen (slot -1 none): the one the
// login entered the world with when the world came next
bool gameUp();
accounts::LastChar entered();
// 3.0.11-beta.176 («Одразу в гру»): enter the world with the character of that name (UTF-8) - its page turned to, its
// place chosen and «Увійти» pressed, a step a frame, whatever [interface] char_select says; no place carries the name
// within 1.5 s (renamed, deleted): the select waits for the player. False: the game's select is not there.
bool enterByName(const std::string& name);

}  // namespace uagfx::win::charselectwin
