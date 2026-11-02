#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::homuncardwin {

// 3.0.11-beta.160 (task 9 of the remaining windows: the homunculus): [interface] homun - the game's homunculus window
// (Alt+R; UI window 0x71 at g_windowMgr + 0x1E4, class 0x6981A0, SendMsg FUN_00467210) as a card (core/homun_card):
// the homunculus as the game keeps it from the server's 0x022e (0x772E38 ...), the window's own buttons pressed
// (message 6: 0xe0 feed, 0xdf its skills, 0xe1 delete - each with the game's own question first -, 0x87 close); the
// game's window hidden (its shown flag 0). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
enum class PadCommand : uint8_t { Left, Right, Press, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::homuncardwin
