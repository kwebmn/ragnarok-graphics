#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::pickcardwin {

// 3.0.11-beta.157 (task 7 of the remaining windows): [interface] picks - the game's «choose a thing» windows in the
// new look (core/pick_card): the magnifier's list of things not known yet (UI window 0x49 at g_windowMgr + 0xAC,
// class 0x699118, SendMsg FUN_0047c580: OK - the game mode's 0x86 with the thing's place) and a card's list of the
// things it goes into (0x4A at + 0xB8, class 0x6991B0, SendMsg FUN_0047cf20: OK - 0x89 with the thing's and the card's
// places). Their list (+0x7C, its count +0x80: a node's ITEM_INFO at +8) read; the chosen one the window's own (+0x68,
// the list's place), OK and cancel its buttons (6 / 0x76, 0x77); the game's window hidden (its shown flag 0). On the
// game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: cancel
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, Ok, Cancel };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::pickcardwin
