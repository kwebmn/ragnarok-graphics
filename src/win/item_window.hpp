#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::itemswin {

// 3.0.11-beta.42 (stage 8 of the new interface, its windows; the owner: «делай окна»): [interface] items - «Речі»
// (Alt+E) redrawn (core/item_window) while the game's own is open: the game's window (UI window 8, g_windowMgr +
// 0x8C, its class's vtable 0x69B8A0) stays made, its shown flag held at 0; Alt+E and the character block's button
// open and close it as before. The bag from the session (a list at +0xF1C: a node's ITEM_INFO at +8), the worn not
// in it; what its own window does, through the same game mode's messages (FUN_004b9a10, FUN_004b98b0): a double
// click uses (0x1D) or wears (0x14), a card into a slot (0x88), arrows (100); a press drags (0x11) as its own; a right
// click its item's card (window 0xC, its message 0x18). Drags let go over it are its window's (win/world_mouse). Its
// place and tab kept in uaro-graphics\items-state.txt. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);  // its search's keys while it has them
// A drag let go at the game's (x, y) over it: given to the game's own window (its drop takes from the storage, the
// cart, the gear); false: not over it
bool drop(int x, int y, uint32_t drag);
bool shown();  // 3.0.11-beta.49: open on the screen now (the world dimmed under it)
// 3.0.11-beta.53: just before the game draws its windows (win/windows_draw) - its own «Речі» hidden, never a frame
void beforeGameDraw();

// 3.0.11-beta.91 (the concept «Контроллер в интерфейсе uaRO», stage 6): played with the controller - the D-pad moves a
// gold frame over the cells (its tip at once, the controller's buttons in it), A uses (as a double click), Y its card,
// X «Призначити» (win/pad_assign), LB / RB the tabs, B closes the game's window
enum class PadCommand : uint8_t { Up, Down, Left, Right, Use, Card, Assign, TabBack, TabOn, Close };
void padCommand(PadCommand c);

}  // namespace uagfx::win::itemswin
