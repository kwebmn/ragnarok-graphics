#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// 3.0.20-beta.70 (the owner's pick of the camera's options, 1: «упираться в настоящие стены и модели»): the 3D
// view's camera tried the way behind the character against the models' BOXES (uaRO.exe's own, +0x130) - a gate's box
// holds its opening, a house's box its roof's overhang and the air beside its corner, and a box the character stands
// in was left out whole, so in a doorway or under an arch the eye went through the walls. Here the models' own
// triangles in the world, as the game drew them once (win/model_census captures every still node for the renderer
// with its corners in the world already): the camera's way and the free camera's step are tried against those. A
// model not drawn yet keeps its box. A face whose picture has holes - leaves, a fence, a grate - is left out: the
// camera goes through a crown. (Not by the «drawn from both sides» flag: of the client's 6382 models' 1.45 million
// faces a quarter carry it, 1726 models on every face - whole houses among them;
// /root/uagfx-tools/audit/camera/twosided.cpp.)
namespace uagfx::solids {

struct Face {
    uint16_t v[3];
    bool open;  // its picture has holes (kOpenHoles percent of it keyed out, or more)
    // 3.0.23-beta.11 (R-CWBD83): an open face's picture place at its corners and its opacity (Set::addMask; -1: none
    // known - the face left out for the bars' sight too)
    float uv[6] = {};
    int16_t mask = -1;
};
// 3.0.23-beta.11: a picture's opacity in kMaskSide x kMaskSide cells, a bit each (opaque: most of its texels' alpha
// 128 or more, as foliage::holesPercent counts them)
constexpr int kMaskSide = 32;
constexpr size_t kMaskBytes = size_t(kMaskSide) * kMaskSide / 8;
void opacityMask(const uint32_t* argb, int width, int height, uint8_t bits[kMaskBytes]);
bool opaqueAt(const uint8_t bits[kMaskBytes], float u, float v);  // wrapped, as the picture repeats
constexpr size_t kMostMasks = 4096;
constexpr int kOpenHoles = 15;
constexpr float kEyeRadius = 3.0f;  // the eye's body for the way behind the character (freeOfSolids)
// a node under this long on the ground both ways - a lamp post, a trunk, a sign - is no wall: in a town they stand at
// every step, and their small boxes were left out before (view3d: a box under 3 half sizes)
constexpr float kThinUnder = 8.0f;

// all the map's still models: Prontera's 30 000 faces are 1 MB; a bound against the unforeseen (36 bytes a triangle)
constexpr size_t kMostTriangles = 300000;
constexpr float kMostCorner = 1e5f;  // a corner past this (or not a number) is no model's: its face left out

class Set {
public:
    void clear();
    // A still node's triangles: `vertices` x y z each in the world, the faces' corners among them. Told again by its
    // id: as it was. A node that would pass kMostTriangles is not taken (has() stays false: its box stands for it).
    void add(uint32_t id, const float* vertices, size_t vertexCount, const Face* faces, size_t faceCount);
    bool has(uint32_t id) const { return known_.count(id) != 0; }
    // The share (0-1] of the way from `from` to `to` at which it first meets a triangle, from either side; -1: none
    float meets(const float from[3], const float to[3]) const;
    // 3.0.23-beta.11: as meets, the faces with holes too where their picture is opaque - what the eye sees through
    float meetsSeen(const float from[3], const float to[3]) const;
    // A picture's opacity known by `key` (its surface): its index for Face::mask; -1 past kMostMasks
    int16_t addMask(uint32_t key, const uint8_t bits[kMaskBytes]);
    int16_t maskOf(uint32_t key) const;
    size_t parts() const { return parts_.size(); }
    size_t triangles() const { return corners_.size() / 9; }
    size_t leftOut() const { return leftOut_; }  // faces of pictures with holes
    size_t seenThrough() const { return openCorners_.size() / 9; }  // of them: their opacity known (the bars)
    size_t thin() const { return thin_; }        // nodes too thin to be a wall

private:
    struct Part {
        float lo[3], hi[3];
        uint32_t first, count;  // triangles
    };
    std::vector<Part> parts_;
    std::vector<float> corners_;  // nine a triangle
    // 3.0.23-beta.11: the faces with holes whose opacity is known - their own parts and corners, a uv and mask each
    std::vector<Part> openParts_;
    std::vector<float> openCorners_, openUv_;  // nine, six a triangle
    std::vector<int16_t> openMask_;
    std::vector<uint8_t> masks_;  // kMaskBytes each
    std::unordered_map<uint32_t, int16_t> maskKeys_;
    std::unordered_map<uint32_t, uint32_t> known_;
    size_t leftOut_ = 0, thin_ = 0;
};

// Behind: the free distance back from the head before the first triangle the way meets (a step before it,
// view3d::kPullNearest at least) - `eye` and `target` as view3d::place made them for `distance`. The eye has a body:
// with `radius` over 0 four more ways from the head end that far up, down, left and right of the eye.
float freeOfSolids(const float eye[3], const float target[3], float distance, const Set& set, float radius);

// The free camera's step from `from` to `to` kept view3d::kFreeGap off the triangles, axis by axis (the world's x, y,
// z: along a wall it slides, up a wall and over the roof); through an opening it goes
void keepOffSolids(const float from[3], float to[3], const Set& set);

}  // namespace uagfx::solids
