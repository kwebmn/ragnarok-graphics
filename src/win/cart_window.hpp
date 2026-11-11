#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::cartwin {

// 3.0.11-beta.154 (task 4 of the remaining windows, the merchant's: the cart): [interface] cart - the merchant's cart
// redrawn as «Візок» (core/item_window's grid: the tabs, the search, the weight of the most, the things of the most in
// its title) while the game's cart window is open (Alt+W or the character window's cart button: UI window 0x28,
// g_windowMgr + 0x140, its class 0x69B770 - made and deleted by the game, its shown flag held at 0 while ours shows
// it). The cart from the session (a list at +0xF4C, a node's ITEM_INFO at +8, as the storage's); its counts the
// server's 0x0121 as the game keeps them (0x77192C: the things, their most, the weight and its most). What the game's
// own window does, through the game mode: a press drags a cart thing (0x59, its place: kind 9) and lets it go where
// the cursor is (0x12) - over «Речі» the bag's drop takes it (the count window for a stack), over «Склад» the
// storage's; a double click gives that drag to «Речі»'s drop at once; a drag of the bag or the storage let go over it
// goes to the game's cart window's drop (FUN_004b7ad0: 0x54 / 0x56, the count window); a right click the item's card;
// its ✕ and Esc delete the game's window (6 / 0x87). Its place and tab kept in uaro-graphics\cart-state.txt. On the
// game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);  // its search's keys while it has them
// A drag let go at the game's (x, y) over it: given to the game's cart window's drop; false: not over it
bool drop(int x, int y, uint32_t drag);
bool shown();
void beforeGameDraw();  // just before the game draws its windows: its cart window hidden
// Played with the controller: the D-pad moves a gold frame over the cells, A takes the thing into the bag, Y its card,
// LB / RB the tabs, B closes the cart window
enum class PadCommand : uint8_t { Up, Down, Left, Right, Take, Card, TabBack, TabOn, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);
void moveTo(int x, int y, bool keep = true);
// 3.0.18-beta.156: the item number at a place of it (0 none) - the «how many» counted by weight
int idAtPlace(int index);
bool put(int index, int count);  // 3.0.18-beta.156: a bag thing put into the cart (the game's own drop)

}  // namespace uagfx::win::cartwin
