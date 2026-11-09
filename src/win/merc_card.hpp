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
// 3.0.16-beta.39: Up / Down over its usable skills' rows, Left / Right a row's use level, Press uses
enum class PadCommand : uint8_t { Press, Close, Up, Down, Left, Right };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

// 3.0.11-beta.216 (the owner: «в интерфейсе нету управления наемниками»): the hired mercenary's block (core/merc_block)
// under the party's panel, or the character block - shown while one is hired (the game's flag 0x772F64 and a
// mercenary's class); a click opens or closes the card as Ctrl+R does (uaRO.exe FUN_004d96e0: DeleteWindow 0x84, else
// MakeWindow 0x84). Under the same [interface] merc.
// 3.0.11-beta.218: its skills as the server listed them (0x029d, core/merc_packet) - the card lists them itself
void skillsSeen(const uint8_t* packet, size_t size);
bool hired();
bool toggleCard();
void blockFrame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void blockDraw(FrameRecord& record, gpu::Renderer& renderer);
bool blockMouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::merccardwin
