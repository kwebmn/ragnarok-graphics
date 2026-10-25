#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
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
static_assert(sizeof(Vertex) == 36 && sizeof(Triangle) == 112, "no padding: compared byte by byte (Cache)");

// A strip's corner: as Vertex; the diffuse alpha how much the place is the other texture's (1.9.5: bilinear between
// the cells' middles, 0.5 at the edge); the specular's blue the way across the strip (0 at the edge), green 255 on the
// side whose noise counts up for the other, red 255 on a cell's own band at its edge (1.9.2).
struct Strip {
    uint32_t inside = 0;  // the group of the cell it lies in: the state, the lightmap (stage 1)
    uint32_t shows = 0;   // the group whose texture (stage 0) it shows
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

// texture: each group's texture (stage 0) - neighbours of one texture whose coordinates go on across their edge look
// alike and get no strip. width: the strips' share of a cell (0-0.5). 2.7.57: laid(x, z), when given, whether the cell
// with its middle at the world's (x, z) lies in a patch the map's makers laid by hand (core/ground_heights.hpp
// laidPatches: Prontera's squares of paving, its mats) - no strip at its edges: the game's straight line.
using Laid = std::function<bool(float x, float z)>;
std::vector<Strip> build(const std::vector<Triangle>& ground, const std::vector<uint32_t>& texture, float width,
                         const Laid& laid = {});

// The cells' side (world units) the triangles show - the median of their extents; 0 when none.
float cellSide(const std::vector<Triangle>& ground);

// 2.5.11: build()'s strips of the last ground, built again only when the ground, its textures or the width differ -
// standing still, the game draws the same ground frame after frame (with Direct3D 11 on the game's own thread).
// 3.0.5-beta.9 (R-VMPQKX: built afresh each frame the camera moved - most of a Radeon 610M laptop's game thread on
// «Високі»): built again, each edge whose cells are as they were takes its strips again - their places and fog from
// this frame's cell, the same as built afresh
class Cache {
public:
    Cache();
    ~Cache();
    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;
    // laidKey: changes as what laid() says changes (another map's patches, its axes found)
    const std::vector<Strip>& build(const std::vector<Triangle>& ground, const std::vector<uint32_t>& texture,
                                    float width, const Laid& laid = {}, uint64_t laidKey = 0);
    void clear();          // another map
    size_t made() const { return made_; }  // builds so far
    size_t reused() const { return reused_; }  // 3.0.5-beta.9: edges whose strips were taken again, so far
private:
    struct Kept;
    std::unique_ptr<Kept> kept_;
    size_t reused_ = 0;
    std::vector<Triangle> ground_;
    std::vector<uint32_t> texture_;
    float width_ = -1.0f;
    uint64_t laidKey_ = 0;
    bool known_ = false;
    std::vector<Strip> strips_;
    size_t made_ = 0;
};

}  // namespace uagfx::seams
