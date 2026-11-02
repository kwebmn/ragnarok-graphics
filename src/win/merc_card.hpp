#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::merccardwin {

// 3.0.11-beta.161 (task 9 of the remaining windows: the mercenary): [interface] merc - the game's mercenary window
// (Ctrl+R; UI window 0x84 at g_windowMgr + 0x1EC, class 0x6982D0, SendMsg FUN_00468100) as a card (core/merc_card):
// the mercenary as the game keeps it (0x772EE0 ...: the contract's end a time_t at 0x772F48), the window's own buttons
// pressed (message 6: 0x11b its skills, 0x11c dismiss - the game asks first -, 0x87 close); the game's window hidden
// (its shown flag 0). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
enum class PadCommand : uint8_t { Press, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::merccardwin
