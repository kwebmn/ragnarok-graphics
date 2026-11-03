#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::msgboxwin {

// 3.0.11-beta.151 (task 3 of the remaining windows, the login's part): [interface] messages - the game's message box
// (UIWindowMgr::ErrorMsg 0x4CEDF0: «Unregistered ID ...», «Disconnected from server», its questions - the box at
// g_windowMgr + 0x70, its class 0x697940, SendMsg FUN_00455b10: message 6 with a button's id ends its own wait) and its
// «Please wait...» (0x4CEFA0, class 0x6978A8) in the new look (core/ask_card) - their words caught as the game asks for
// them, their buttons pressing its own; on the login's screens too. On the game's thread.
void install();  // the two calls' words caught (their first instructions to a copy, as the chat's)
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);  // Enter and Esc: the box's own answers to them
bool shown();
bool asking();  // 3.0.11-beta.203: its card shown asking or telling - not «Please wait» (win/login_card's veil)
bool close();  // Esc from the windows' stack
void beforeGameDraw();
enum class PadCommand : uint8_t { First, Second };  // A, B
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::msgboxwin
