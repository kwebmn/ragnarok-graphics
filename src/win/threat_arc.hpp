#pragma once
#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::win::threatarcwin {

// 3.0.19-beta.36 (the 3D view's concept: «Индикатор угрозы: красная дуга у края экрана с той стороны, откуда бьют»):
// [view3d] threat - in the 3D view, for each monster that hit the character in the last core threatarc::kHoldMs and
// stands past the view's sides, a red arc at the screen's edge on its side (core/threat_arc), fading; drawn once a
// frame before the interface, over the world. On the game's thread.
void setOn(bool on);
void packet(const packets::Event& e);
void newFrame();
void draw(FrameRecord& record, int width, int height);
// 3.0.19-beta.37 (the owner: «надо писать растояние пускай клетка будет метр»): each arc's attacker's distance by it
// (uaUI; k the interface's scale, the target's size) - its frame after draw's
void labels(double k, int targetWidth, int targetHeight);

}  // namespace uagfx::win::threatarcwin
