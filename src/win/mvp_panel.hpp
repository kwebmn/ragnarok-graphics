#pragma once
#include <windows.h>

#include <vector>

#include "core/frame.hpp"
#include "core/game_packets.hpp"
#include "core/mvp_timers.hpp"
#include "win/pad_panels.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::mvpwin {

// 3.0.11-beta.280 (the owner's feature 9): [interface] mvp_timers - «MVP», a panel of the interface (its place in
// «Розміщення інтерфейсу») while a boss the character saw die is on its way back: each one's name, map and the time
// left by the server's rule (core/mvp_timers; the respawn from the reference pack's spawns), «+» once it is back; a
// click on a row forgets it. A boss: the pack's boss flag, its respawn there 10 minutes or more; its death the
// 0x0080 «died» of its actor (the class read before the game takes the packet). Kept in uaro-graphics\mvp.txt.
void packet(const packets::Event& e);
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

// 3.0.18-beta.242 (the reference's boss cards: «Ваші таймери» first): the timers the panel shows now, the soonest first
std::vector<mvptimers::Timer> timers();
// 3.0.18-beta.135 (win/pad_panels): each timer's row (forgotten by a press) and ✕ for the controller
std::vector<padpanels::Button> padButtons();

}  // namespace uagfx::win::mvpwin
