#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

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

}  // namespace uagfx::win::mvpwin
