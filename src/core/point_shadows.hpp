#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::point_shadows {

// 3.0.16-beta.16 (the owner, from Korangar's ideas: «тени от факелов и ламп» - «начинай»): [effects] torch_shadows -
// the torches' and the map's lamps' light stopped by what stands between: each of the few lights that matter most
// sees the frame's opaque world (the sun map's casters, sun_map.cpp) round it as a cube of six faces of depth, kept in
// one atlas - a row of six faces a light; the torches' light pass (torch_accum_ps) asks the face the way to a pixel
// goes through. The faces' axes are the world's, their x and y the next two axes round (shaders.cpp torchShadow).
constexpr int kMostLights = 8;   // the lights with shadows in a frame
constexpr int kFaceTexels = 256; // a face's side
constexpr float kNearIgnore = 3.0f;  // world units: what lies this near the light (its own holder) casts nothing

// Face f (0-5): the axis f / 2, towards + for an even f; right and up the next two axes round.
struct Face {
    float fwd[3], right[3], up[3];
};
Face face(int f);
// The face a way from the light (world) goes through and where on it, -1..1 each way; false for no way at all
bool faceOf(const float d[3], int& f, float& u, float& v);
// Whether a box (its corners less the light's place, world) can show on face f - never false for one that does
bool boxOnFace(const float lo[3], const float hi[3], int f);

// A light the pass draws: where (view) and how far it reaches, how strong - the picks are those that light most of
// the screen (strength x reach over the distance), at most kMostLights; their indices into `lights`
struct Candidate {
    float view[3];
    float reach;
    float strength;
};
std::vector<int> pick(const std::vector<Candidate>& lights, float nearZ, int most = kMostLights);

}  // namespace uagfx::point_shadows
