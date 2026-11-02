#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::petcardwin {

// 3.0.11-beta.159 (task 9 of the remaining windows: the pet): [interface] pet - the game's pet window (Alt+J; UI
// window 0x58 at g_windowMgr + 0x1B4, class 0x697FD8, SendMsg FUN_00464c50) as a card (core/pet_card): the pet's name,
// level, fullness and closeness as the game keeps them (0x771A78 ...), its commands the window's own menu choices
// (message 0x27: feed - the game asks first -, perform, the accessory off, back into the egg), ✕ its close (6 / 0x87);
// the game's window hidden (its shown flag 0). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
enum class PadCommand : uint8_t { Left, Right, Up, Down, Press, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::petcardwin
