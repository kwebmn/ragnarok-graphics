#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::commandwin {

// 3.0.11-beta.206 (the tester's third list: «alt j - command list в старому стилі»): [interface] commands - the game's
// command list (Alt+Y: UI window 0x70 at g_windowMgr + 0x1E0, class 0x69A2F0, SendMsg FUN_0048eee0: 6 / 0x87 closes)
// as a card (core/command_card): its lists from the session (0x771148 + 0x1910 the switches, + 0x1920 the commands,
// + 0x1930 the switches' states, + 0x1940 the windows' keys - pairs of words and a code each), a click doing what the
// game's click does (FUN_0048e400): a command the game mode's message 0x2F with its code (/hi with its three empty
// words), a window the windows' manager's key (FUN_004d96e0) as Alt and its letter; the game's window hidden while the
// card shows. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::commandwin
