#pragma once

#include "core/exp_gain.hpp"
#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::expgainwin {

// 3.0.11-beta.237 (the owner: «когда убиваешь моба … над мобом было типа + столько то опыта»): [interface] exp_gain -
// the experience a kill gave rising over the monster that died (core/exp_gain), and its label by the experience's
// bars at the bottom (win/exp_bars). On the game's thread (the packets as the game takes them).
void packet(const packets::Event& e);
void frame(bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
// The label by the bars: the last gain and when (its ms 0: none, or the switch off)
expgain::Label label();

}  // namespace uagfx::win::expgainwin
