#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::askcardwin {

// 3.0.11-beta.150 (task 2 of the remaining windows): [interface] asks - the game's small windows in the new look
// (core/ask_card): a trade's request (UI window 0x20 at g_windowMgr + 0x110), a party's (0x23, + 0x120), a guild's
// (0x46, + 0x134) and a friend's (0x6D, + 0x1DC) - their own buttons pressed (message 6: 0x76 accept, 0x77 decline),
// the names from the server's packets; an NPC's number (0x38, + 0x160) or text (0x64, + 0x164) to type - the game's
// window moved off the screen (its field keeps the keys and the controller's typing), its words shown as its field has
// them, «Готово» pressing its OK. On the game's thread.
void packet(const packets::Event& e);
// 3.0.11-beta.155: `counts` [interface] counts - the game's «how many» window (UI window 0xF at g_windowMgr + 0xA0) as
// a count card: what for, the most, the game's field, one / half / all (set and OK), OK its own, «Скасувати» ends the
// game's drag and hides its window (it has no cancel of its own)
void frame(HWND game, bool on, bool counts, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.18-beta.190 (the tester: Enter put nothing into «Склад», «Готово» all of it): Enter on a count whose field is
// empty - the most put in as «Готово» puts it, the key left to the game (its own Enter, its OK); false always
bool key(UINT m, WPARAM wp, LPARAM lp);
bool shown();
bool asking();
// 3.0.18-beta.155: the game's «how many» just made (a stack moved in a batch) - its whole count set and its OK
// pressed; false: none shown
bool countAll();  // a request or a count shown (an NPC's input the controller leaves to the game's own field)
bool close();   // Esc: a request declined, a count given up (true); an NPC's input left to the game
void beforeGameDraw();
enum class PadCommand : uint8_t { Accept, Decline };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::askcardwin
