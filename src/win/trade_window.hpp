#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::tradewin {

// 3.0.11-beta.116 (the windows plan's stage 7; the concept «Концепция окон и инвентаря», stage 4): [interface] trade -
// the trade between players redrawn as «Обмін» (core/trade_window) while the game's trade window is open: its window
// (UI window 0x1F, g_windowMgr + 0x10C, its class's vtable 0x699A98) stays made, its shown flag held at 0, and «Речі»
// opens beside it. Both sides from the session (ten ITEM_INFOs each at +0x1290 and +0x1560, the zeny offered at
// +0x1830 and +0x1834), the partner's name and the locks from the game's window (+0xA0, +0x70 / +0x74 / +0x78 /
// +0x7C). What the game's window does, through its own buttons (its SendMsg FUN_004840c0, message 6): «Готово» 0x76
// (the zeny typed put in its field first: it sends the zeny, then the lock), «Обмін» 0xA4, «Скасувати» 0x77, Enter in
// the zeny field 0xA0; a drag of the bag let go over it, or «Речі»'s double click, goes to its drop (FUN_004848a0: the
// count window for a stack). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);  // the zeny field's keys while it has them
bool drop(int x, int y, uint32_t drag);  // a drag let go over it: the game's trade window's drop
bool shown();
void beforeGameDraw();  // its game's window hidden, «Речі» made when it opened
// A thing of the bag (its place, its count) offered as a drag let go over it does - «Речі»'s double click and the
// controller's A while «Обмін» is open; false: not open, or your offer is locked
bool put(int index, int count);

// Played with the controller: the D-pad moves a gold frame over both sides' cells, A the next step («Готово», then
// «Обмін»), Y the card, B cancels the trade
enum class PadCommand : uint8_t { Up, Down, Left, Right, Next, Card, Cancel };
void padCommand(PadCommand c);

bool box(int& x, int& y, int& w, int& h);
void moveTo(int x, int y, bool keep = true);
bool moving();

}  // namespace uagfx::win::tradewin
