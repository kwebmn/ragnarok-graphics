#pragma once
#include <cstdint>
#include <vector>

#include "core/gnd.hpp"
#include "core/terrain.hpp"

namespace uagfx::sun_shadows {

// 3.0.20-beta.50 (the owner's «делай 1-9», 2 - a trial of real shadows in the 3D view): the houses', the trees' and
// the ground's own shadows from the map's own models and ground, whole on the card - a map of depths seen from the
// sun's side over a box round the eye (kTexels a side, kHalf each way along the light's own right and up, kDepth
// towards the sun and away), laid over the picture by each pixel's place in it. In the map file's places (the own
// ground's, the models' and OwnGround::toLight's).
// 2.1.2-2.1.4's «Тени от солнца» and «Настоящие тени» were made of what the frame showed - «крутились и прыгали
// вместе с камерой» - and lay over the map's painted shadows - «спорили с тенями, которые игра уже нарисовала». These
// are of the whole map's geometry, their box moved by whole texels; and where they are drawn the painted ones are
// taken out of the ground's light (sunlessAtlas)
constexpr int kTexels = 2048;
constexpr float kHalf = 320.0f, kDepth = 1500.0f;
struct Box {
    float right[3] = {1.0f, 0.0f, 0.0f}, up[3] = {0.0f, 0.0f, 1.0f}, toSun[3] = {0.0f, -1.0f, 0.0f};
    float middle[3] = {0.0f, 0.0f, 0.0f};
    float half = kHalf, depth = kDepth;
};
// the light's own axes for the way to the sun (a unit; up along -y): its right along the ground, its up square to both
void axesOf(const float toSun[3], float right[3], float up[3]);
// the box round `focus`: its middle the focus moved onto the map's texels along the light's right and up - the
// shadows' edges stand still while the eye walks
Box boxAt(const float focus[3], const float toSun[3], float half, float depth, int texels);
// a place in the box: where in the map (0-1 across, 0-1 down) and how deep (0 the sun's side, 1 the far one)
void placeIn(const Box& b, const float w[3], float out[3]);
// where the eye's box is centred: half of `half` before the eye along the ground the way it looks (more of the box in
// sight), at the eye's own height; looking straight down: the eye's place
void focusOf(const float eye[3], const float forward[3], float half, float out[3]);
// how dark the map's makers painted their shadows: the darkest tenth of its ground's light (its tops' lightmaps'
// brightness) over the level most of it lies at (the 70th of a hundred) - a real shadow as dark, kShadeLeast to
// kShadeMost; no shadow painted or nothing known: kShadeUsual
constexpr float kShadeUsual = 0.6f, kShadeLeast = 0.35f, kShadeMost = 0.85f;
float shadeOf(const gnd::Ground& g);
// the ground's light without its painted shadows: the atlas again, its tops' lightmaps (and the mean cell after them
// all, the land's past the edges) no darker than the level most of the ground lies at; their coloured light and the
// walls' own lightmaps as they were. What a renderer keeps of the map for it: which lightmaps are tops' (one more at
// the end: the mean cell) and that level - it makes the rows it puts up from the atlas itself
struct Sunless {
    std::vector<uint8_t> tops;
    uint32_t lit = 255;
};
Sunless sunlessOf(const gnd::Ground& g);
// rows `row` .. `row + rows` of the atlas without the painted shadows, into out (the atlas's width a row)
void sunlessRows(const terrain::Atlas& atlas, const Sunless& s, int row, int rows, uint32_t* out);
terrain::Atlas sunlessAtlas(const gnd::Ground& g, const terrain::Atlas& atlas);

}  // namespace uagfx::sun_shadows
