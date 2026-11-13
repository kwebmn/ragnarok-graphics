#pragma once
#include <cstddef>
#include <functional>
#include <vector>

#include "core/seams.hpp"

namespace uagfx::ground_cells {

// 2.2.1: where the map's own ground is - for each cell of it, from the last frame's ground triangles
// (core/seams.hpp's), the heights its own faces span (the cliffs' walls left out). The grass grows on a green pixel
// only when it lies on that ground: umbala's wooden decks and bridges, green as they are, stand above it and grew
// grass and flowers.
struct Grid {
    float originX = 0.0f, originZ = 0.0f;  // the world's place of cell (0, 0)'s corner
    float side = 0.0f;                     // a cell's side (world units); 0: no grid
    int width = 0, height = 0;             // cells
    std::vector<float> span;               // width * height pairs, row by row: the lowest and the highest y, or
                                           // lowest > highest: no ground there
};

constexpr int kMostSide = 256;  // cells a side at most: the texture's

// The grid about the triangles' middle, at most kMostSide a side.
Grid build(const std::vector<seams::Triangle>& ground);
// 3.0.18-beta.252 (R-9UNG8S «в замке трава плоская с отражением»): the cells with ground whose middle (the world's x,
// z) `off` names - a hall's floor, its texture of a folder «...내부» (ground_heights::interiorTexture) - left with no
// ground: no grass there (a castle's green carpet, Payon's green tiles). The number of cells so taken out.
size_t dropWhere(Grid& g, const std::function<bool(float x, float z)>& off);

}  // namespace uagfx::ground_cells
