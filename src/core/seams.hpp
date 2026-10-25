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

// 3.0.5-beta.13 (the user: «Швы на видеокарте» - with Direct3D 11 the strips were still most of a Radeon 610M
// laptop's game thread on «Високі»: made, placed and turned by the camera on the processor each frame): each edge's
// two pieces made once, in the world's places, into slots of the card's memory; a frame says only which pieces are
// in view and their cells' fog, and the card puts the camera on them (seam_vs). An edge is known by its cell's place
// on the map's grid and its way, not by the frame's groups - the game's draws of the cells are numbered anew as the
// view moves, and beta.9's Cache, which knew its edges by them, took almost nothing again in the game.

// A piece's vertex as the card reads it: three uint4 (seam_vs)
struct CardVertex {
    float pos[3] = {0, 0, 0};  // the world (up along -y)
    uint32_t diffuse = 0;      // as Vertex's
    float uv0[2] = {0, 0}, uv1[2] = {0, 0};
    uint32_t specular = 0;     // as Vertex's without the fog (alpha 0): the shown piece's corners give it
    uint16_t share[4] = {0, 0, 0, 0};  // each of the cell's corners' share of the fog, 65535 all of it
    uint32_t spare = 0;
};
static_assert(sizeof(CardVertex) == 48, "three uint4 (seam_vs)");

// The slots' two sizes: a piece whose triangles the crease cut none of (15 vertices, 16 triangles), and one it cut
enum Size : uint32_t { kPlain = 0, kCut = 1 };
constexpr uint32_t kSlotVertices[2] = {15, 47};
constexpr uint32_t kSlotIndices[2] = {48, 144};

// A piece in view, in the order drawn: each edge's piece of the neighbour's texture, then its cell's own band
struct Shown {
    uint32_t slot = 0;
    uint32_t size = kPlain;
    uint32_t indices = 0;            // its triangles' corners: the first ones of its slot's
    uint32_t inside = 0, shows = 0;  // the groups of this frame, as a Strip's
    uint32_t fog = 0;                // its cell's corners' fog this frame: corner k's specular alpha in bits 8k
};

class Store {
public:
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    // depth: a place's depth in the view, (x, y, z, 1) dot depth - an edge whose cell reaches nearZ is left out
    // (applySeams left out a strip with a corner there). laidKey as Cache's.
    void build(const std::vector<Triangle>& ground, const std::vector<uint32_t>& texture, float width,
               const float depth[4], float nearZ, const Laid& laid = {}, uint64_t laidKey = 0);
    const std::vector<Shown>& shown() const { return shown_; }
    // The slots the last build filled, of each size: their numbers, and their contents one slot after another
    // (kSlotVertices and kSlotIndices of each, the indices from the slot's first vertex) - for the card
    const std::vector<uint32_t>& freshSlots(uint32_t size) const;
    const std::vector<CardVertex>& freshVertices(uint32_t size) const;
    const std::vector<uint16_t>& freshIndices(uint32_t size) const;
    uint32_t slotsUsed(uint32_t size) const;  // the card's room for the slots of a size: [0, this)
    void clear();          // another map, or the card's memory lost: every piece made anew
    size_t made() const;   // pieces made so far
private:
    struct Kept;
    std::unique_ptr<Kept> kept_;
    std::vector<Shown> shown_;
    std::vector<Triangle> ground_;  // the last build's (Cache's): the same again - nothing to do
    std::vector<uint32_t> texture_;
    float width_ = -1.0f, depth_[4] = {0, 0, 0, 0}, nearZ_ = 0.0f;
    uint64_t laidKey_ = 0;
    bool known_ = false;
};

}  // namespace uagfx::seams
