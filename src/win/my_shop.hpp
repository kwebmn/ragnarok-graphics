#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::myshopwin {

// 3.0.11-beta.166 (task 4 of the remaining windows, the merchant's: one's own open shop): [interface] my_shop - the
// game's own-shop window (UI window 0x2D at g_windowMgr + 0x150, class 0x69B478, SendMsg FUN_004b0a60) as a card
// (core/my_shop): the things still for sale as the session keeps them (+0xF7C: a node's ITEM_INFO at +8, how many
// +0x10, the price +0x18), the shop's sign (0x772D98, a char*), «Закрити лавку» its own close (6 / 0x87: the game
// closes the shop); the game's window hidden (its shown flag 0). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::myshopwin
