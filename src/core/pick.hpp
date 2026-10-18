#pragma once
#include <functional>

#include "core/frame.hpp"

namespace uagfx::pick {

// 2.3, uaRO Studio (L2): what the game drew under the cursor - its draws are pre-transformed (TL) vertices, x and y
// at each vertex's start in the game's pixels.
bool inTriangle(float ax, float ay, float bx, float by, float cx, float cy, float x, float y);  // edges in
// The last of a frame's draws (the one on top) wanted whose triangles hold (x, y) - lists, strips and fans, indexed
// or not; points and lines never. -1 none.
int lastDrawAt(const FrameRecord& r, float x, float y, const std::function<bool(const DrawCmd&)>& wanted);

}  // namespace uagfx::pick
