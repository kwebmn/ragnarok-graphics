#pragma once
#include "core/buff_timers.hpp"
#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::buffoverlay {

// 3.0.4-beta.6 (the user's idea 4): the time left on the player's buff icons (core/buff_timers.hpp). The casts and
// the icons from the game's packets (win/packet_tap.cpp); the icons where uaRO.exe lays them (FUN_005d56a0: from the
// screen's right edge, 35 pixels apart, five passes by its own CSession::IconImage 0x648bc0); the time on each icon's
// foot, in the game's picture under its cursor ([buffs] timers).
void packet(const packets::Event& e);
// Each frame: on, the target's pixels per game pixel
void frame(bool on, float k);
// At the scene's end (not in the photo mode)
void draw(FrameRecord& record, gpu::Renderer& renderer);
// 3.0.7-beta.6: the character's buffs counted (whether shown or not) - a buff ending told (win/notifier.cpp)
const buffs::Timers& timers();

}  // namespace uagfx::win::buffoverlay
