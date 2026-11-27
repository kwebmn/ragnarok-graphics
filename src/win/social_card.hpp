#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::socialcardwin {

// 3.0.11-beta.163 (tasks 5 and 6 of the remaining windows: the friends and the party): [interface] social - the game's
// friends and party window (Alt+H or the party's button; UI window 0x22 at g_windowMgr + 0x11C, class 0x69BBB4,
// SendMsg FUN_004c2250) as a card (core/social_card): its tab (+0xC0: 0 the friends, 1 the party) and its list (+0xA4:
// a node's name +0x18, its map +0x28, the leader +0x34 == 0, offline +0x38) read; a row chosen as the window's own
// (+0xAC); the tabs and the actions its own buttons (6 / 0x95, 0xa6, 0xa9, 0xaa - the game asks first -, 0xa8, 0x87);
// the game's window hidden (its shown flag 0). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
// 3.0.24-beta.27 ([interface] party_invite, the testers' box): «Запросити в групу» on a friend in the game, not of
// our party - our own packet 0x00fc with their account from the server's friends' list (rawPacket: 0x0201, 0x020a)
void rawPacket(uint16_t id, const uint8_t* p);
void setInvite(bool on);
bool close();  // Esc: its close
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, Left, Right, Press, TabBack, TabOn, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::socialcardwin
