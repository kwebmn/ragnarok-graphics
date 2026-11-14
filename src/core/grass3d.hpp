#pragma once
#include <cstdint>

namespace uagfx::grass3d {

// 3.0.19-beta.33 (the owner: «кстати, я так траву в 3д и не увидел»; the 3D view's concept: «объёмная трава с
// цветами»): the 3D view's grass - blades in the world, not grass_ps's lengths in the picture's pixels made for the
// game's own distance. The renderers' grass3d_vs makes a triangle a blade, its root on what the picture shows under
// it, on a grid the world's own round the eye (the blades stay put as it moves): side x side of them, step apart,
// thinned past 60 % of its reach. Fewer and further apart for a weak card or keep_fps (EffectSettings::grassThin).
constexpr int kSide = 220, kSideThin = 140;
constexpr float kStep = 0.5f, kStepThin = 0.6f;
// 3.0.19-beta.38 (R-ZMGYMM «трава есть но слабая и практически не видно», R-WGTX3Z «травы нету»): a tuft of kTuft
// blades a root, taller, turned to the camera, darker at the root and lighter at the tip - one thin blade of the
// ground's own colour was lost in it
constexpr uint32_t kTuft = 3;
struct Grid {
    int side = kSide;
    float step = kStep;
    uint32_t blades() const { return uint32_t(side) * uint32_t(side); }  // the roots
    uint32_t vertices() const { return blades() * 3u * kTuft; }
    float reach() const { return float(side / 2) * step; }  // from the eye
};
inline Grid gridFor(bool thin) { return thin ? Grid{kSideThin, kStepThin} : Grid{kSide, kStep}; }

}  // namespace uagfx::grass3d
