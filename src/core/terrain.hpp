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
    uint32_t color;   // ARGB, the surface's; a neighbour's strip: alpha how much the place is the neighbour's
    uint32_t blend;   // 3.0.4-beta.12, a strip's: as the ground seams' specular (shade() bit 31) - red 255 a cell's own
                      // band, green 255 on the side whose noise counts up, blue the way across (0 at the edge)
    float plain;      // 3.0.4-beta.29: the plain ground's height here (its cell's two triangles, from (x + 1, y) to
                      // (x, y + 1)) - the smooth ground eased onto it before a chunk changes to the plain one
};
// 3.0.4-beta.12 (the user chose «Мягкие переходы»): the layers of a chunk, drawn in this order - the ground; along each
// edge between two cells that look different (another texture, or one whose coordinates jump there) and no step,
// inside each: its own band (its texture again over the edge, kept off the texture's outer texels, whole at the edge,
// fading - the rim's dark line covered), then the neighbour's strip (its texture mirrored across the edge over the
// cell's own rows up to the middle, on its vertices exactly, shown where the noise over the world says - one wandering
// line both sides agree on), as the game's ground seams (core/seams.hpp) on the game's cells
constexpr uint8_t kGround = 0, kOwnBand = 1, kNeighbour = 2;
// 3.0.4-beta.10: a batch's vertices lie together and its indices count from its first - one draw of 16-bit indices
struct Batch {
    int texture = -1;  // the .gnd's texture
    uint8_t layer = kGround;
    uint32_t firstVertex = 0, vertexCount = 0;
    uint32_t firstIndex = 0, indexCount = 0;
};
// kChunk x kChunk cells with their own vertices and batches - the renderer projects those in view alone (3.0.4-beta.10:
// 8, not 16 - prt_fild08 at the game's zoom 300 had 1481 of the 40096 vertices of the chunks in view on the screen)
constexpr int kChunk = 8;
struct Chunk {
    int cellX = 0, cellY = 0;  // 3.0.4-beta.12: its first cell
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
// laid: a cell row by row 1 in a patch the map's makers laid by hand (core/ground_heights.hpp laidPatches) - no strips
// round it, its edges as straight as the game draws them; the strips with `subdivide` 2 and more alone (none far off).
// 3.0.6-beta.3 (R-RSK2JR «дирка на сходах в комодо»): resting, a cell row by row 1 where a model rests on it
// (restingOn) - the smooth ground there no higher than the game's plain cells (the patch rose over the steps of
// cmd_fild02's fortress stairs, 0.1 over their slope's cells); a point a cell shares takes it from either cell
Mesh build(const gnd::Ground& g, int subdivide, const Atlas& atlas, const std::vector<uint8_t>* laid = nullptr,
           const std::vector<uint8_t>* resting = nullptr);
// 3.0.6-beta.3: the cells a model's face lies on - from kRestReach over its plain cells to kRestUnder under them (the
// patch rises at most a few units over them) - the models placed in the ground's places (own_models::place)
constexpr float kRestReach = 4.0f, kRestUnder = 1.0f;
std::vector<uint8_t> restingOn(const gnd::Ground& g, const Mesh& models);

// 3.0.4-beta.20 (the own ground on every map): the map's water as uaRO.exe lays it (0x4e59d0, core/water.hpp) - a
// quad over each cell with a top a corner of which lies under the water's highest (its level less the waves' height;
// up along -y), flat at the level (the waves' rise and fall, a unit or so, left out), its corners' texture coordinates
// the cell's place modulo four a quarter each; by the ground's chunks, one batch each (the water's one texture)
Mesh water(const gnd::Ground& g, float level, float waveHeight);

}  // namespace uagfx::terrain
