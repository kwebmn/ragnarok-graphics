#pragma once
#include <windows.h>

#include <cstdint>
#include <string>

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

// 3.0.11-beta.220: the server's guild packets for tabs 2-5 (0x0160, 0x0162, 0x0163, 0x0166, 0x016f) - the packet tap
void packet(uint16_t id, const uint8_t* data, size_t size);
// 3.0.11-beta.219 (the tester: «на великій мапі ... пишіть нік замість "Член гільдії"»): a guild mate's name by its
// account (0x01eb's) from the game's members' list (a member's first field its account, its second its character);
// "" when not in it
std::string mateName(uint32_t account);  // in the game's code page as its frame was told it
// 3.0.11-beta.263 (the tester: «в список гільдії беремо портрети без необхідності бачити согільдійця, чому б просто не
// перевикористати логіку?»): a guild mate's looks from the game's members' list (0x0154: its hair, the hair's colour,
// sex, class, level); false - no such mate
struct MateLook {
    int sex = 0, hair = 0, colour = 0, job = 0, level = 0;
};
bool mateLook(uint32_t account, MateLook& out);

}  // namespace uagfx::win::guildcardwin
