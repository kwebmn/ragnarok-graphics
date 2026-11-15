#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/bridge_wire.hpp"

// 3.0.20-beta.3 (stage 2a of «Модели нашим рендером», Claude Doc XMkZ5UMHQXV9CJCvsy82j6): a static model node of the
// game's as it draws it, captured by the driver once a map (win/model_census) and sent to the renderer - its vertices
// in the world, its faces with their texture coordinates and colours as the game keeps them (core/model_census), its
// textures as the renderer knows them.
namespace uagfx::gamemodels {

constexpr uint8_t kTwoSided = 1;

struct Face {
    uint16_t v[3] = {};       // the node's vertices
    float uv[6] = {};         // as the game draws them (its +0x118, scaled to the texture's own part)
    uint32_t colour[3] = {};  // the game's +0x118 colours (its plain and other ways)
    int32_t lit[9] = {};      // its +0x144 ints, r g b a corner (its lit way: the colour plus its place's light)
    uint8_t texture = 0;      // into Node::textures
    uint8_t flags = 0;        // kTwoSided
};
static_assert(sizeof(Face) == 84, "the same bytes in the driver's 32 bits and the renderer's 64");

struct Node {
    uint32_t id = 0;   // the game's node (its address, one a map)
    uint32_t map = 0;  // the map's serial it was captured on
    std::vector<uint32_t> textures;  // the renderer's texture ids
    std::vector<float> vertices;     // x y z in the world each
    std::vector<Face> faces;
    // 3.0.20-beta.4: its actor's scale below 0 on an odd number of axes (the game's draw's second argument, its actor's
    // render FUN_004E0B20): its faces' turn on the screen the other way round
    bool flipped = false;
};

void write(bridge::WireWriter& w, const Node& n);
bool read(bridge::WireReader& r, Node& n);  // false: cut short or out of reason

// 3.0.20-beta.4 (stage 2b): a frame's nodes the renderer draws in place of the game's (the driver left them out of the
// game's draw), the game's colour factors (0x7710E4/E8/EC) and whether it lights them by their places (0x7710B9)
struct Frame {
    std::vector<uint32_t> ids;
    float factors[3] = {1.0f, 1.0f, 1.0f};
    bool lit = false;
};
void write(bridge::WireWriter& w, const Frame& f);
bool read(bridge::WireReader& r, Frame& f);

// 3.0.20-beta.4: whether the game's draw shows a face (0x4DED2E) - its corners' x y on the screen `a`, `b`, `c`: a
// one-sided face by its turn, the other way round on a flipped node; a two-sided one always
bool shown(const float a[2], const float b[2], const float c[2], bool twoSided, bool flipped);

// 3.0.20-beta.5: whether a node's keys move it - `count` keys `stride` bytes apart, each one's value `valueBytes` long
// at `valueAt` in it: one differing from the first's (the frames alone move nothing)
bool keysMove(const uint8_t* keys, size_t count, size_t stride, size_t valueAt, size_t valueBytes);
// 3.0.20-beta.5: two placing matrices the same within the float noise of the view's inverse - the rows of the turn and
// the scale 1e-3 apart at most, the move 0.01
bool samePlace(const float a[12], const float b[12]);

// 4 x 3 matrices as the game keeps them (rows: x, y, z, the move; a point p -> p x m): `a` then `b`
void compose(const float a[12], const float b[12], float out[12]);
// a node's world matrix from the game's node-to-view one and the view's inverse
void worldOf(const float combined[12], const float viewInverse[12], float world[12]);
void transform(const float m[12], const float p[3], float out[3]);

// Corners by a grid of cells `cell` units wide: whether a point lies within `reach` (at most a cell) of one - the
// renderer's check of the captured nodes against our own models placed from the map's files
class Corners {
public:
    explicit Corners(float cell = 1.0f) : cell_(cell) {}
    void add(const float p[3]);
    bool within(const float p[3], float reach) const;
    size_t size() const { return size_; }
    void clear() {
        cells_.clear();
        size_ = 0;
    }

private:
    uint64_t key(int x, int y, int z) const;
    std::unordered_map<uint64_t, std::vector<std::array<float, 3>>> cells_;
    float cell_;
    size_t size_ = 0;
};

}  // namespace uagfx::gamemodels
