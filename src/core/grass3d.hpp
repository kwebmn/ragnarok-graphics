#pragma once
#include <cmath>
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
// 3.0.20-beta.47 (the owner's «делай 1-9», 1 - «Трава-луг»; R-YAB82P «трава гораздо лучше, но в 3д она не
// смотрится»: short straight triangles of one height and the ground's own colour - a brush): the meadow. A tuft of
// bent blades - kBladesNear of kLinksNear links in the first ring, kBladesFar of kLinksFar further off (a link a quad,
// the last a point); the field's own clumps and bald spots (tallOf, chanceOf by its noise), the blades going down to
// nothing by the green's edge (edgeOf), a root's colour that of the ground round it. A root is found once a frame, a
// texel of the roots' picture (side wide, a ring a band of side rows; the renderers' meadow_roots_ps) - beta.33-46
// searched it anew for each of a tuft's nine vertices. Looked at on prt_fild08's own ground pictures before the
// shader: /root/uagfx-tools/audit/meadow/meadow_sim.py
constexpr uint32_t kBladesNear = 4, kBladesFar = 3;
constexpr uint32_t kLinksNear = 3, kLinksFar = 2;
constexpr uint32_t bladeVertices(uint32_t links) { return links * 6u - 3u; }
// 3.0.20-beta.25 (R-6YX3UF «трава в 3д режимі трошки далі не видна і якась вона не дуже»: none past some 55 units, its
// edge thinned at random): rings of roots round the eye - the first as the grid was, each further one twice as far
// out, its roots twice as far apart (side x side of them again, those of the rings within left out).
// 3.0.20-beta.27 (R-TBD86F «когда идешь трава двигается не естественно»; the owner: «и трава не такая как вид
// сверху»): beta.25's rings had roots of their own, their blades twice and four times as wide and taller - walking, a
// ring's edge went along with the eye and the grass at a place changed its blades as it crossed. The roots now the top
// view's (grass_ps: a half unit's cell of the world each, in the same spot); a ring takes the cells of its own step's
// corners alone - the nearer rings' roots in the same spots, a level each (levelOf) -, the finer ones growing in round
// them as the eye nears (presenceOf); a blade's width and height going with the distance, not the ring (grass3d_vs)
constexpr int kRings = 3, kRingsThin = 2;
struct Grid {
    int side = kSide;
    float step = kStep;
    int rings = kRings;
    uint32_t roots() const { return uint32_t(side) * uint32_t(side); }  // a ring's
    uint32_t blades() const { return roots() * uint32_t(rings); }        // the roots of them all
    uint32_t tuftVertices(int ring) const {
        return ring == 0 ? kBladesNear * bladeVertices(kLinksNear) : kBladesFar * bladeVertices(kLinksFar);
    }
    uint32_t vertices() const { return roots() * (tuftVertices(0) + tuftVertices(1) * uint32_t(rings - 1)); }
    float stepOf(int ring) const { return step * float(1 << ring); }
    float fullTo() const { return float(side / 4) * step; }  // every root up to it
    float reachOf(int ring) const { return float(side / 2) * stepOf(ring); }  // from the eye
    float reach() const { return reachOf(rings - 1); }
};
inline Grid gridFor(bool thin) { return thin ? Grid{kSideThin, kStepThin, kRingsThin} : Grid{kSide, kStep, kRings}; }
// 3.0.20-beta.47: which corner a vertex is - the furthest ring's tufts first (the painter's order: the near blades
// over the far), a tuft's blades in turn, a blade's quads from its root (two triangles each), then its point. joint:
// 0 the root ... the links' number the tip; side: -1, 1, 0 the tip itself
struct Corner {
    int ring = 0;
    uint32_t tuft = 0, member = 0, joint = 0;
    int side = 0;
};
inline Corner cornerOf(const Grid& g, uint32_t vertex) {
    Corner c;
    // (not "far": windef.h makes it nothing)
    const uint32_t ringVertices = g.roots() * g.tuftVertices(1), outer = ringVertices * uint32_t(g.rings - 1);
    uint32_t local = vertex - outer;
    if (vertex < outer) {
        const uint32_t turn = vertex / ringVertices;
        c.ring = g.rings - 1 - int(turn);
        local = vertex - turn * ringVertices;
    }
    const uint32_t links = c.ring == 0 ? kLinksNear : kLinksFar;
    const uint32_t perBlade = bladeVertices(links), perTuft = g.tuftVertices(c.ring);
    c.tuft = local / perTuft;
    const uint32_t v = local % perTuft, k = v % perBlade;
    c.member = v / perBlade;
    if (k >= 6u * (links - 1u)) {  // its point: the last joint's two corners, then the tip
        const uint32_t t = k - 6u * (links - 1u);
        c.joint = t == 2u ? links : links - 1u;
        c.side = t == 0u ? -1 : t == 1u ? 1 : 0;
    } else {
        const int joints[6] = {0, 0, 1, 1, 0, 1}, sides[6] = {-1, 1, -1, -1, 1, 1};
        c.joint = k / 6u + uint32_t(joints[k % 6u]);
        c.side = sides[k % 6u];
    }
    return c;
}
// the root (its column and row on a ring's grid) of the tuft drawn n-th: row by row from the furthest, the rows
// across the way the eye looks (forwardX, forwardZ: the view's forward along the ground)
inline void tuftAt(int side, uint32_t n, float forwardX, float forwardZ, int& column, int& row) {
    const int a = int(n % uint32_t(side)), b = int(n / uint32_t(side));
    if (std::fabs(forwardZ) >= std::fabs(forwardX)) {
        column = a;
        row = forwardZ >= 0.0f ? side - 1 - b : b;
    } else {
        row = a;
        column = forwardX >= 0.0f ? side - 1 - b : b;
    }
}
// three bytes in one float's 24 bits (the roots' picture: a root's colour; its hue, flower and the steps' push)
inline float packBytes(int a, int b, int c) { return float(a + b * 256 + c * 65536); }
inline void unpackBytes(float packed, int out[3]) {
    // a whole number as it is: half added to one past 2^23 rounds it off (floats there a unit apart)
    const int v = int(packed);
    out[0] = v & 255;
    out[1] = (v >> 8) & 255;
    out[2] = (v >> 16) & 255;
}
// the field's own shape by its noise (0-1): a tuft's height from kTallLow in its bald spots to kTallHigh in its
// clumps (a character stands some 16 tall), three roots of four left in the bald spots
constexpr float kTallLow = 1.6f, kTallHigh = 4.6f, kThin = 0.75f;
inline float tallOf(float clump) { return kTallLow + (kTallHigh - kTallLow) * clump * std::sqrt(clump); }
inline float chanceOf(float clump) { return kThin + (1.0f - kThin) * clump; }
// a root's height's share by the share of green ground round it (its own place and four places 1.6 off): none under
// a quarter, whole from 0.85 - beta.33-46's blades stood whole up to the last green pixel, a wall along the sand
constexpr float kEdgeFrom = 0.25f, kEdgeBand = 0.6f, kEdgeOff = 1.6f;
inline float edgeOf(float share) {
    const float a = (share - kEdgeFrom) / kEdgeBand;
    return a <= 0.0f ? 0.0f : a >= 1.0f ? 1.0f : a;
}
// the fourth blade of a first-ring tuft: whole near, gone before the ring's edge (the next ring draws three)
inline float fourthOf(const Grid& g, float dist) {
    const float a = (dist - g.fullTo() * 1.25f) / (g.fullTo() * 0.55f);
    const float t = a <= 0.0f ? 0.0f : a >= 1.0f ? 1.0f : a;
    return 1.0f - t * t * (3.0f - 2.0f * t);
}
// the ring a root `dist` from the eye is of; -1 past them all
inline int ringOf(const Grid& g, float dist) {
    for (int k = 0; k < g.rings; ++k)
        if (dist < g.reachOf(k)) return k;
    return -1;
}
// a root's level: the times its cell (cx, cz) halves evenly both ways - the trailing zero bits -, at most `most`
inline int levelOf(int cx, int cz, int most) {
    int level = 0;
    while (level < most && ((cx >> level) & 1) == 0 && ((cz >> level) & 1) == 0) ++level;
    return level;
}
// the level wanted `dist` from the eye: 0 up to `full`, one more each time it doubles
inline float needOf(float dist, float full) { return dist <= full ? 0.0f : std::log2(dist / full); }
// how much of a root of `level` stands where `need` is wanted: whole up to its level, none a level past it; the last
// level's going over half a level
inline float presenceOf(int level, float need, int rings) {
    const float a = level + 1 >= rings ? (float(rings) - need) * 2.0f : float(level) + 1.0f - need;
    return a <= 0.0f ? 0.0f : a >= 1.0f ? 1.0f : a;
}
// a blade's width (times the nearest's) - as wide on the screen past `full` - and height (a third more a level wanted)
inline float widthAt(float dist, float full) { return dist <= full ? 1.0f : dist / full; }
inline float heightAt(float need, int rings) {
    return 1.0f + 0.35f * (need < float(rings - 1) ? need : float(rings - 1));
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
// 3.0.20-beta.27: at most kMostWider times (a blade seen nearly edge on), its coverage the same - alpha 1 / wider
constexpr float kMinPixels = 2.0f, kMostWider = 6.0f;  // 3.0.20-beta.29: 2 (R-DWKJEN: side-on blades faded)
inline void fadeOf(float pixels, float& wider, float& alpha) {
    if (pixels >= kMinPixels) {
        wider = alpha = 1.0f;
        return;
    }
    if (pixels <= 0.0f) {
        wider = 1.0f;
        alpha = 0.0f;
        return;
    }
    wider = kMinPixels / pixels < kMostWider ? kMinPixels / pixels : kMostWider;
    alpha = 1.0f / wider;
}

}  // namespace uagfx::grass3d
