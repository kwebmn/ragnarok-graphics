#pragma once
#include <cstdint>
#include <vector>

#include "core/gnd.hpp"

namespace uagfx::terrain {

// 3.0.4-beta.9 (the user: «сделай 1» - the own world's prototype on prt_fild08): the ground as the driver draws it
// itself from the map's .gnd (core/gnd.hpp) in place of the game's cells. Its places are the world's (the maps' axes
// x +1, z +1): cell (x, y)'s corner at ((x - width / 2) * zoom, height, (y - height / 2) * zoom), up along -y.
// Smooth where the ground is whole - each cell cut into `subdivide` x `subdivide` with a Catmull-Rom patch over the
// corners around it, through the game's own corner heights - its steps (a wall, a cliff) kept as the game has them.
struct Vertex {
    float pos[3];
    float normal[3];  // up is -y
    float uv[2];      // its texture's
    float lm[2];      // the lightmap atlas'
    uint32_t color;   // ARGB, the surface's
};
// 3.0.4-beta.10: a batch's vertices lie together and its indices count from its first - one draw of 16-bit indices
struct Batch {
    int texture = -1;  // the .gnd's texture
    uint32_t firstVertex = 0, vertexCount = 0;
    uint32_t firstIndex = 0, indexCount = 0;
};
// kChunk x kChunk cells with their own vertices and batches - the renderer projects those in view alone (3.0.4-beta.10:
// 8, not 16 - prt_fild08 at the game's zoom 300 had 1481 of the 40096 vertices of the chunks in view on the screen)
constexpr int kChunk = 8;
struct Chunk {
    float lo[3], hi[3];  // its box in the world
    uint32_t firstVertex = 0, vertexCount = 0;
    uint32_t firstBatch = 0, batchCount = 0;
};
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;  // each its batch's
    std::vector<Batch> batches;  // a chunk's, one a texture
    std::vector<Chunk> chunks;
};

// The lightmaps side by side, 8 x 8 each, as the game packs them (FUN_004e32e5): brightness in alpha, colour in RGB;
// a lightmap's 6 x 6 middle sampled, its edge texels around it
constexpr int kAtlasWidth = 2048;
struct Atlas {
    int width = 0, height = 0;
    std::vector<uint32_t> argb;
};
Atlas lightmapAtlas(const gnd::Ground& g);

constexpr float kJoin = 0.5f;  // corners closer than this are one: whole ground
Mesh build(const gnd::Ground& g, int subdivide, const Atlas& atlas);

}  // namespace uagfx::terrain
