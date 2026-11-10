#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::vendsetupwin {

// 3.0.16-beta.33 (the owner: «делай» - the shop's opening in the new style): [interface] vend_setup - the game's two
// vending windows as one card (core/vend_setup), the game's own hidden (their shown flags 0). uaRO.exe's: the shop
// being made, UI window 0x29 at g_windowMgr + 0x148 (class 0x69BA68, SendMsg FUN_004bfe40: 6 / 0x76 its OK - the
// prices checked, the game mode's 0x5B with the sign builds 0x01B2 -, 6 / 0x77 cancel, 0x26 a drop), its sign's edit
// at +0x70, a price's edit at +0x7C + 4 a place, the places at +0xB0; the cart's things one may put up, UI window
// 0x2A at + 0x144 (class 0x69B808, SendMsg FUN_004b8760: 0x26 a drop); the session's lists of both (+0xF70 the
// cart's, +0xF94 the sale's: a node's ITEM_INFO at +8 - its place +8, the edit of the n-th sale the n-th); the game
// mode's 0x5D begins a cart thing's drag (kind 0xB, its place), 0x5E a sale's (kind 10); an edit's words
// FUN_00443300, the keys' edit the manager's + 0x6C (FUN_004ce510 gives it them). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: «Скасувати»
// 3.0.18-beta.105 (the owner: «Tab/Enter к следующей цене»): while the name or a price has the keys - Tab the next
// field (Shift+Tab the one before, round), Enter the next price (stays on the last); their WM_CHAR swallowed
bool key(UINT m, WPARAM wp, LPARAM lp);
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);
// 3.0.16-beta.39 (the owner: «геймпад» - the new cards on the pad): the D-pad's left and right the cart's list or the
// sale's, up and down a row (a gold edge); A puts the cart's thing up (on a sale: its price field takes the keys), X
// takes a sale back, LB / RB its price a step down / up, Y «Відкрити лавку», B «Скасувати»
enum class PadCommand : uint8_t { Up, Down, Left, Right, Put, Take, Less, More, Open, Cancel };
void padCommand(PadCommand c);

}  // namespace uagfx::win::vendsetupwin
