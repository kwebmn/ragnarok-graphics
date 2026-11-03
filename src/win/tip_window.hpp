#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::tipwin {

// 3.0.11-beta.204 (the tester's third list: «Tip of the Day» left in the game's old look): [interface] tips - the
// game's tip window (UI window 0x26 at g_windowMgr + 0x13C, class 0x699540, SendMsg FUN_0047fa80: message 6 with 0xB4
// back, 0x98 next, 0x87 close, 0x93 its start box with the new state) as a card (core/tip_card): the tip read from
// the game's list (tipOfTheday.txt, at 0x74B9F8: + 8 the first of its tips, each a list of lines), its number from
// the window's + 0x64, the box from + 0x68; the game's window hidden while the card shows. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);  // ← and → a tip back and on (the chat's line empty)
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
enum class PadCommand : uint8_t { Back, Next, Close, Startup };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::tipwin
