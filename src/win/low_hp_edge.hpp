#pragma once

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::lowhpedge {

// 3.0.11-beta.50 (the concept's low HP: «кромка экрана — по выбору, выключена»): [interface] low_hp_edge 0-100 -
// while the character's HP is under a quarter (as the character block's red) the screen's edges glow red, the per
// cent its strength (0 none, the default); over the world, under the interface (put where the world ends in the
// record), in over 180 ms, out over 240. On the game's thread.
void frame(int strength, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::lowhpedge
