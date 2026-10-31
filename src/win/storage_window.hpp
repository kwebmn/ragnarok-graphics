#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::storagewin {

// 3.0.11-beta.115 (the windows plan's stage 6; the concept «Концепция окон и инвентаря», stage 4: «сумка рядом,
// перетаскивание с количеством»): [interface] storage - the Kafra storage redrawn as «Склад» (core/item_window's grid:
// the tabs, the search, the places used of the most) while the game's storage window is open: its window (UI window
// 0x21, g_windowMgr + 0x114, its class's vtable 0x69B938) stays made, its shown flag held at 0, and «Речі» opens beside
// it. The storage from the session (a list at +0xF40, a node's ITEM_INFO at +8, as the bag's). What the game's own
// windows do, through the game mode's messages: a press drags a storage thing (0x3C, its place and count) and lets it
// go where the cursor is (0x12) - over «Речі» its window's drop takes it into the bag (the game's count window for a
// stack, FUN_004ba530); a double click gives that drag to the same drop at once; a drag of the bag let go over it, or
// «Речі»'s double click, goes to the game's storage window's drop (FUN_004bb6c0: the count window, the game's checks);
// a right click the item's card (window 0xC); its ✕ and Esc the window's close (6 / 0x87: the game mode's 0x3F). Its
// place and tab kept in uaro-graphics\storage-state.txt. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);  // its search's keys while it has them
// A drag let go at the game's (x, y) over it: given to the game's storage window's drop; false: not over it
bool drop(int x, int y, uint32_t drag);
bool shown();
// Just before the game draws its windows (win/windows_draw): its storage window hidden, «Речі» made when it opened
void beforeGameDraw();
void packet(const packets::Event& e);  // 0x00f2: the places used of the most
// A thing of the bag (its place, its count) put into the storage as a drag let go over it does - the game's count
// window for a stack: «Речі»'s double click and the controller's A while «Склад» is open; false: not open
bool put(int index, int count);

// Played with the controller: the D-pad moves a gold frame over the cells, A takes the thing into the bag, Y its card,
// LB / RB the tabs, B closes the storage
enum class PadCommand : uint8_t { Up, Down, Left, Right, Take, Card, TabBack, TabOn, Close };
void padCommand(PadCommand c);

// Where it is on the target (win/window_stack: placed beside the others, docked with «Речі»); false not shown
bool box(int& x, int& y, int& w, int& h);
void moveTo(int x, int y, bool keep = true);  // `keep` false: it follows «Речі» being dragged
bool moving();  // its title dragged now

}  // namespace uagfx::win::storagewin
