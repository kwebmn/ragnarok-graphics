#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::optionscardwin {

// 3.0.11-beta.162 (task 8 of the remaining windows: the game's options): [interface] options - the game's options
// window (Alt+O; UI window 0xD at g_windowMgr + 0x98, class 0x698830, SendMsg FUN_00473dd0) as a card
// (core/options_card): the switches as the game keeps them (the sounds 0x6B8320, the music off 0x772D28, the attack's,
// the skills', the things' snap 0x77196C / 0x771968 / 0x771960, opaque windows 0x77195C), the volumes the window's
// (+0x74 the music, +0x70 the sounds); a switch pressed as its own checkbox does (6 / 0x93 with its new state, then
// the window's 0x3c refresh), a volume as its slider (message 8 by the step), ✕ its close (6 / 0x87); the game's
// window hidden (its shown flag 0). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, Left, Right, Press, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::optionscardwin
