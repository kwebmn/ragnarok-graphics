#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::partysetupwin {

// 3.0.11-beta.158 (task 6 of the remaining windows: the party's settings): [interface] party_setup - the game's
// «Config Party» window (Alt+P; UI window 0x35 at g_windowMgr + 0x158, class 0x699B30, SendMsg FUN_00485650) as a
// card (core/party_setup): its three choices read from the window, a click presses that choice's own radio button
// (message 6 / 0x95 from it - the game's leader check and its chat line kept), OK and cancel its buttons (0x76: the
// game mode's 0xb6 for a party being made, 0x6d for the experience changed by the leader); the game's window hidden
// (its shown flag 0). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: cancel
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, Left, Right, Ok, Cancel };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::partysetupwin
