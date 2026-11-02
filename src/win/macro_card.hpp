#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::macrocardwin {

// 3.0.11-beta.169 (task 8 of the remaining windows: the chat's macros): [interface] macros - the game's macro window
// (Alt+M; UI window 0x56 at g_windowMgr + 0x1A8, class 0x69C288, SendMsg FUN_004534c0) as a card (core/macro_card):
// its ten fields (+0x64 on: an edit control each - its words FUN_00443300) shown as they are, a click giving that field
// the keys (the manager's SetFocus FUN_004ce510), «Зберегти» its Enter (message 0: FUN_00453a10 writes them), ✕ its
// close (6 / 0x87); the game's window moved off the screen (its fields keep the keys). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::macrocardwin
