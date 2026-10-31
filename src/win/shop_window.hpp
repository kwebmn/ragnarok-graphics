#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::shopwin {

// 3.0.11-beta.100 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's
// «Магазин»): [interface] shop - the NPC's shop (core/shop_window) in place of the game's deal windows, their shown
// flags held at 0: the deal's choice (UI window 0x19 at the manager's +0xF8, class 0x699410 - its own «Купити» pressed
// at once, the NPC kept from its +0x60), the shop's list (0x16, +0xEC) with the buy cart (0x17, +0xF0) or the sell
// cart (0x18, +0xF4). The wares are the game's: the session's deal list (+0xF28: ITEM_INFOs, the price +0x18 after
// Discount / Overcharge, the bag's place +8 and count +0x10 when sold). The cart goes into the game's own purchase or
// sale list through its own functions (FUN_00629750 / FUN_0062a4b0, the ITEM_INFO's count +0x10 set for the call) and
// the cart window's own OK sends it (6 / 0x76: the game mode's 0x25 / 0x26, packets 0x00c8 / 0x00c9); its cancel
// (0x77) closes the deal; the other tab: the cancel, then the game mode's 0x28 / 0x29 with the NPC (packet 0x00c5).
// The weight from the reference pack, the NPC's name from its shops. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.11-beta.101: the keys while the chat's line is empty - the arrows (Page Up / Down, Home, End) move the frame,
// left and right or Tab between the list and the cart, Space or + one more, - one fewer (Shift+Space ten), the digits
// the amount, Del takes it back, F1 its description (the game's item card), Enter buys or sells, Esc closes; its
// search's keys
bool key(UINT m, WPARAM wp, LPARAM lp);
void beforeGameDraw();
bool shown();

// The controller's (pad runtime shopPad): up and down the list or the cart, left and right between them, A one more,
// X one fewer (held: on and on, `times` at once), LT takes it back, Y its description, Start buys or sells, LB / RB the
// tabs, B closes the typing, the card or the deal
enum class PadCommand : uint8_t { Up, Down, Left, Right, Plus, Minus, TakeBack, Info, Confirm, TabBack, TabOn, Close };
void padCommand(PadCommand c, int times = 1);

}  // namespace uagfx::win::shopwin
