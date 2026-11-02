#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::servercardwin {

// 3.0.11-beta.164 (task 3 of the remaining windows, the login's last: the server select): [interface] servers - the
// game's server select (UI window 2: in the manager's windows' list only - g_windowMgr + 0x38, its class 0x6979D8,
// SendMsg FUN_004563c0) as a card (core/pick_card's): its list control (+0x60: its items +0x58 - 16-byte strings -,
// their end +0x5C, the chosen one +0x64) read, a row chosen as the control's own, «Далі» and «Назад» its buttons (6 /
// 0x76 - the game mode's message with the server's place -, 0x77 back to the login); the game's window off the screen.
// On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, Ok, Back };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::servercardwin
