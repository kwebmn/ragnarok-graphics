#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::vendshopwin {

// 3.0.11-beta.156 (task 4 of the remaining windows, the merchant's: buying from another's shop): [interface] vending -
// the game's two windows of a merchant's shop (its things: UI window 0x2B at g_windowMgr + 0x14C, class 0x69B510;
// the purchase: 0x2C at + 0x154, class 0x69B640 - both made at the server's 0x0133 and deleted at «Купити» or
// «Скасувати») as one card «Лавка» (core/vend_shop), both held hidden (their shown flags 0). The shop's things from the
// session (+0xF64), the purchase too (+0xF88 - the list the game's «Купити» sends, 0x0134): + and − change it through
// the session's own (FUN_00629da0 / FUN_00629e90, each with the thing's ITEM_INFO and the count), the game's checks
// kept (no more than the shop has, no more than the zeny pays); «Купити» and «Скасувати» its purchase window's own
// buttons (0x76 / 0x77). Its title the shop's sign (0x0131). On the game's thread.
void packet(const packets::Event& e);
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: «Скасувати»
void beforeGameDraw();
// The controller: up and down a row, A one more of it, X one less, Y buys, B gives up
enum class PadCommand : uint8_t { Up, Down, More, Less, Buy, Cancel };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::vendshopwin
