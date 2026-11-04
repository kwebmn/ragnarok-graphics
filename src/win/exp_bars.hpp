#pragma once

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::expbars {

// 3.0.11-beta.236 (the concept «Блок персонажа»: «внизу экрана показывать опыт бейз/джоб полосками» - «включить»):
// [interface] exp_bars - the base's and the job's experience as two thin bars along the screen's bottom edge, over the
// world and the interface; pointed at, a tip over them with each one's per cent, numbers, pace and time to the level.
// Gone at the last levels, outside the game's world and while the character block is off (its numbers). On the game's
// thread.
void frame(bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::expbars
