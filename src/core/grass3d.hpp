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
// 3.0.20-beta.25 (R-6YX3UF «трава в 3д режимі трошки далі не видна і якась вона не дуже»: none past some 55 units, its
// edge thinned at random): rings of roots round the eye - the first as the grid was, each further one twice as far
// out, its roots twice as far apart (side x side of them again, those of the rings within left out), its blades wider
// by as much and a little taller; a ring's outer part thinned to the next one's density, the last one's to none with
// its blades shrinking to nothing (grass3d_vs the same sums)
constexpr int kRings = 3, kRingsThin = 2;
struct Grid {
    int side = kSide;
    float step = kStep;
    int rings = kRings;
    uint32_t roots() const { return uint32_t(side) * uint32_t(side); }  // a ring's
    uint32_t blades() const { return roots() * uint32_t(rings); }        // the roots of them all
    uint32_t vertices() const { return blades() * 3u * kTuft; }
    float stepOf(int ring) const { return step * float(1 << ring); }
    float reachOf(int ring) const { return float(side / 2) * stepOf(ring); }  // from the eye
    float reach() const { return reachOf(rings - 1); }
};
inline Grid gridFor(bool thin) { return thin ? Grid{kSideThin, kStepThin, kRingsThin} : Grid{kSide, kStep, kRings}; }
// the ring a root `dist` from the eye is of; -1 past them all
inline int ringOf(const Grid& g, float dist) {
    for (int k = 0; k < g.rings; ++k)
        if (dist < g.reachOf(k)) return k;
    return -1;
}
// the chance a root of `ring` is kept: all of them inside 60 % of its reach, then fewer to the next ring's density at
// its edge (a quarter), the last ring's to none
inline float keepOf(const Grid& g, int ring, float dist) {
    const float reach = g.reachOf(ring);
    const float t = dist <= reach * 0.6f ? 0.0f : dist >= reach ? 1.0f : (dist - reach * 0.6f) / (reach * 0.4f);
    return 1.0f - t * (ring + 1 >= g.rings ? 1.0f : 0.75f);
}
inline float widthOf(int ring) { return float(1 << ring); }
inline float heightOf(const Grid& g, int ring, float dist) {
    const float tall = 1.0f + 0.35f * float(ring);
    if (ring + 1 < g.rings) return tall;
    const float reach = g.reachOf(ring), left = (reach - dist) / (reach * 0.25f);
    return tall * (left <= 0.0f ? 0.0f : left >= 1.0f ? 1.0f : left);
}
// 3.0.20-beta.25: green ground for a blade - prt_fild08's olive meadow (its green 0.99-1.03 of its red) in, its sand
// (0.94-0.96) out; the 3.0.19-beta.33 test (1.04 of the red) left the meadow in patches. 3.0.20-beta.26 (R-PKPE9D
// «впечатление что трава мигает»: beta.25's edge at 0.985 ran through the meadow - a root whose pixel crossed it as the
// camera moved came and went): how green, 0 to 1 - none below 0.96 of its red or 1.08 of its blue, all past 1.01 and
// 1.16 (grass3d_vs bladeGreen)
inline float greenness(float r, float g, float b) {
    const float byRed = (g / (r > 1e-4f ? r : 1e-4f) - 0.96f) / 0.05f;
    const float byBlue = (g / (b > 1e-4f ? b : 1e-4f) - 1.08f) / 0.08f;
    float a = byRed < byBlue ? byRed : byBlue;
    const float byLight = (g - 0.08f) / 0.04f;
    a = a < byLight ? a : byLight;
    return a <= 0.0f ? 0.0f : a >= 1.0f ? 1.0f : a;
}
// 3.0.20-beta.26: a root by an edge (of the green ground, of a ring's thinning) grows and shrinks with how far inside
// it lies, never comes and goes whole - its height's share by its chance p (0-1) and its own number u (0-1): all whole
// at p 1, none at 0, those just inside small (grass3d_vs bladeGrow)
constexpr float kGrowBand = 0.15f;
inline float growOf(float p, float u) {
    const float a = (p * (1.0f + kGrowBand) - u) / kGrowBand;
    return a <= 0.0f ? 0.0f : a >= 1.0f ? 1.0f : a;
}
// 3.0.20-beta.26 (R-PKPE9D, under DLSS's jitter): a blade narrower than kMinPixels on the target drawn that wide, as
// much fainter - a far blade under two pixels wide flickered from frame to frame
constexpr float kMinPixels = 2.5f;
inline void fadeOf(float pixels, float& wider, float& alpha) {
    if (pixels >= kMinPixels) {
        wider = alpha = 1.0f;
        return;
    }
    wider = kMinPixels / (pixels > 1e-3f ? pixels : 1e-3f);
    alpha = pixels > 0.0f ? pixels / kMinPixels : 0.0f;
}

}  // namespace uagfx::grass3d
