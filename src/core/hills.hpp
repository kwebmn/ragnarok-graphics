#pragma once
#include <cstdint>
#include <vector>

#include "core/seams.hpp"

namespace uagfx::hills {

// 1.15, the remaster's smooth hills: the game lights its ground cell by cell, each a flat face, and a hill shows as
// steps of light. From the ground's triangles in the world (the last frame's, core/seams.hpp's) a normal for each
// corner of the cells - the faces around it averaged, the walls of the cliffs left out - which the shader reads
// between the corners (shaders.cpp hillLight): the sun lights the slopes smoothly, as a modern terrain's.
struct Grid {
    float originX = 0.0f, originZ = 0.0f;  // the world's place of corner (0, 0)
    float side = 0.0f;                     // between two corners (world units); 0: no grid
    int width = 0, height = 0;             // corners
    std::vector<float> packed;             // width * height, row by row: pack() of each corner's normal, 0 none
};

constexpr int kMostSide = 256;  // corners a side at most: the texture's

// The normal (up along -y, |n| = 1) as one float a float texture holds exactly: 4096 * (1 + x's 11 bits) + z's 12
// bits; y is the rest of the unit length, upwards. 0 is no normal.
float pack(float nx, float nz);
bool unpack(float v, float& nx, float& ny, float& nz);

// The grid around the triangles' middle, at most kMostSide a side; smoothed once over its neighbours.
Grid build(const std::vector<seams::Triangle>& ground);

}  // namespace uagfx::hills
