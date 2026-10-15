#pragma once
#include <cstdint>

namespace uagfx::foliage {

// 0.64: the leaves the wind sways ([effects] foliage): a texture of the world that is mostly green and has holes
// (leaves cut out by the colour key or alpha) - not a fence, a banner or a wall. 0xAARRGGBB pixels, alpha 0 a hole.
bool leafy(const uint32_t* argb, int width, int height);

// How far a vertex at game y moves across (game pixels) at time t: nothing at the draw's bottom, `amplitude` swing at
// its top; slow waves over the screen, so that neighbouring trees move a little apart.
float sway(float x, float y, float bottom, float height, float amplitude, float t);

}  // namespace uagfx::foliage
