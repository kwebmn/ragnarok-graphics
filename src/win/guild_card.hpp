#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::guildcardwin {

// 3.0.11-beta.165 (task 6 of the remaining windows: the guild): [interface] guild - the game's guild window (Alt+G;
// each tab a UI window of its own: 0x3C-0x41 at g_windowMgr + 0x170 on, 4 apart) as a card (core/guild_card): the
// guild's information as the game keeps it (its object at 0x74B650: the name, the master, the territory as strings,
// the numbers from the server's 0x01b6) and its members (that object's list +0xBC: a node's name +0x14, job +0x28,
// title +0x38, level +0x44, online +0x60); the open tab's window hidden (its shown flag 0), ✕ its close (6 / 0x87).
// On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, TabBack, TabOn, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::guildcardwin
