#pragma once
#include <cstdint>
#include <deque>
#include <vector>

#include "core/footsteps.hpp"

// 2.6.8: the steps' hold on the grass - how far each spot of the ground has its blades pushed aside and trodden down
// by the last steps - in a small grid over the ground around them, made on the processor once a frame. grass_ps went
// through up to 64 steps for every pixel (two reads and some twenty sums each): on an HD 6570 the grass cost 22 ms a
// frame and keep_fps turned it off ten seconds after it came (R-9K2F6E); now it reads the grid, four texels.
namespace uagfx::trample {

// What the steps do to the blades at a spot of the ground: the way they lean (along the world's x and z, its length
// how hard) and the share of their height left (1 none trodden, 0 flat).
struct Hold {
    float pushX = 0.0f, pushZ = 0.0f;
    float left = 1.0f;
};

// The hold as grass_ps summed it until now: each step within sqrt(12) units pushes the blades away from it and
// within 3 treads them down, the newer the more - age (now - born) / life, 0 just left .. 1 gone. life 0: none hold.
Hold at(const std::deque<footsteps::Step>& steps, float now, float life, float x, float z);

struct Grid {
    static constexpr int kSide = 128;            // texels a side
    static constexpr float kReach = 3.4641016f;  // sqrt(12): how far a step pushes
    float originX = 0.0f, originZ = 0.0f;        // the world at the grid's corner (texel (0, 0)'s outer corner)
    float cell = 0.5f;                           // world units a texel's side
    // kSide * kSide texels, row by row along z, each four bytes for DXGI_FORMAT_R8G8B8A8_SNORM: the push along x and z
    // halved (-127..127 for -2..2), the share left (0..127), 0; empty: no step holds anything
    std::vector<int8_t> texels;
    std::vector<float> sums;  // the build's own: the push and the share left of every texel before they are bytes
};

// The grid over the steps that still hold (their bounds and a step's reach around them, a texel at least half a
// unit, the edge's texels holding nothing): the shader reads past it the edge's - nothing.
void build(const std::deque<footsteps::Step>& steps, float now, float life, Grid& out);

// 2.7.29: on which rows grass_ps looks for the blades' roots (uiPixels.z): every second at twice the game's size and
// more - a game pixel two rows and more (4K), the same blades for half the work - else every row.
inline int rootRows(float scale) { return scale >= 1.5f ? 2 : 1; }

// The grid read at a spot as grass_ps reads it (between the four texel centres around, by how near each is; past
// the edge its edge) - for the tests; an empty grid holds nothing.
Hold sample(const Grid& g, float x, float z);

}  // namespace uagfx::trample
