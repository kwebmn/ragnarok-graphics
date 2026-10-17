#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::seams {

// 1.9, the remaster's ground: the game draws its ground cell by cell, each with one texture, and where a grass cell
// meets a road's the line runs straight along the cells - a staircase. From the ground's triangles in the world
// (the game's draws turned back by its camera) the cells are found, and where two neighbours look different a strip
// inside each along their edge shows the other's texture - mirrored across their edge (1.9.1) -
// through a noise both strips share (shaders.cpp, bit 31): the line between them wanders over the world.
struct Vertex {
    float pos[3] = {0, 0, 0};  // the world (up along -y)
    uint32_t diffuse = 0xffffffffu, specular = 0;
    float uv0[2] = {0, 0}, uv1[2] = {0, 0};
};

struct Triangle {
    Vertex v[3];
    uint32_t group = 0;  // the caller's: its draw's state and textures
};

// A strip's corner: as Vertex, the diffuse alpha 255 at the edge and 0 at the strip's inner side, the specular's green
// 255 on the side that shows the other where the noise is low, 0 on the other.
struct Strip {
    uint32_t inside = 0;  // the group of the cell it lies in: the state, the lightmap (stage 1)
    uint32_t shows = 0;   // the group whose texture (stage 0) it shows
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

// texture: each group's texture (stage 0) - neighbours of one texture whose coordinates go on across their edge look
// alike and get no strip. width: the strips' share of a cell (0-0.5).
std::vector<Strip> build(const std::vector<Triangle>& ground, const std::vector<uint32_t>& texture, float width);

// The cells' side (world units) the triangles show - the median of their extents; 0 when none.
float cellSide(const std::vector<Triangle>& ground);

}  // namespace uagfx::seams
