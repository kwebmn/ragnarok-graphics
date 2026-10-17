#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uagfx::smooth_shade {

// 1.19, smooth light on the models (effects.smooth_models): the game lights a house's, a rock's model face by face -
// each triangle one colour, a rock a heap of flat facets. A draw lit so (most of its triangles one colour each) gets
// each corner's colour averaged with the corners at the same place on the faces around it that turn from its own by
// less than `crease` (a cosine): curved stone and bark round off, a house's walls and roof keep their edges. The
// game's own light, only spread.
struct Corner {  // a vertex of the game's (its screen place, its colour) - those at one place share x, y, rhw exactly
    float x = 0, y = 0, rhw = 0;
    uint32_t colour = 0;  // A8R8G8B8
};

struct Change {
    uint32_t vertex;
    uint32_t colour;
};

// The draw's vertices and its triangles (indices into them, three a triangle); f0, f1, cx, cy the projection (the
// view's place of a vertex). Empty when the draw is not lit face by face, or nothing changes.
std::vector<Change> smooth(const std::vector<Corner>& vertices, const std::vector<uint32_t>& triangles, float f0,
                           float f1, float cx, float cy, float crease = 0.8f);

}  // namespace uagfx::smooth_shade
