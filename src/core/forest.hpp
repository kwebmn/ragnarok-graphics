#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/rsm.hpp"

namespace uagfx::forest {

// 2.0, the new trees from the map (effects.new_trees): the map file names each tree and where it stands, its model
// file gives its size - the trunk (its narrow part) and the crown (the rest). The driver grows its own tree of that
// size and kind once a map - a trunk with branches and a crown of hundreds of leaf cards (a fir's tiers, a palm's
// fronds) - and draws all of them in a few calls, the game's model left out.
enum class Kind : uint8_t { None, Broadleaf, Conifer, Palm, Bush, Willow };

// By the model file's name as the map names it (cp949 bytes, "나무잡초꽃\나무01.rsm"): trees, firs, palms, willows,
// bushes; not a tree's stand, post, piece, stump or plank, not bamboo.
Kind kindOf(const std::string& file);

struct Shape {
    Kind kind = Kind::None;
    float height = 0;        // the model's top above the ground (world units, up positive)
    float trunkTop = 0;      // where the trunk goes into the crown (above the ground)
    float trunkRadius = 0;   // at its foot
    float crownLo[3] = {0, 0, 0}, crownHi[3] = {0, 0, 0};  // as the model file's boxes: up along -y
};

// The shape of a model of that kind; false when it is too small or has no crown to grow.
bool shapeOf(Kind kind, const rsm::Model& model, Shape& out);

// A vertex of a grown tree, in the model's room (up along -y, the foot at 0): its place, the light's normal, the
// texture's place (the bark's, or the card's 0-1), how open to the sky (0-1), how much the wind moves it, its part
// (0 bark, 1 a card of leaves, 2 a palm's frond, 3 a fir's twig) and a number of its own (0-1).
struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
    float open;
    float sway;
    float part;
    float seed;
};
static_assert(sizeof(Vertex) == 48, "the forest's vertex");

struct Mesh {
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
};

// The tree of a shape, its own by the seed (the same seed, the same tree).
Mesh grow(const Shape& shape, uint32_t seed);

// A picture's colour (0-1): the average of its opaque pixels (alpha 128 and more), the game's A8R8G8B8; false when
// it has none.
bool averageColour(const uint32_t* argb, int width, int height, float out[3]);

}  // namespace uagfx::forest
