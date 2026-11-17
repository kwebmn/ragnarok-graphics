#pragma once
#include <cstdint>

namespace uagfx::foliage {

// 0.64: the leaves the wind sways ([effects] foliage): a texture of the world that is mostly green and has holes
// (leaves cut out by the colour key or alpha) - not a fence, a banner or a wall. 0xAARRGGBB pixels, alpha 0 a hole.
bool leafy(const uint32_t* argb, int width, int height);
// 2.6.19: whether a texture's whole picture is a tree's leaves - leafy(), but never a page of the game's sprite cache
// (`spriteCache`): it holds the characters', monsters' and items' frames (the Pyramids' green Poporings came out
// leaves and shed them).
bool leafyTexture(const uint32_t* argb, int width, int height, bool spriteCache);
// 3.0.20-beta.61: how much of a picture is holes (alpha under a half), in percent - the models' dump tells it of a
// texture whose face is not in the picture (a place on a hole is no proof of a face not drawn)
int holesPercent(const uint32_t* argb, int width, int height);

// 2.7.30: the swing at a tree's top at foliage_strength 100 % (game pixels) - 1.2 until 2.7.29, a pixel or two of the
// picture (the tester in Payon's forest: «Листя на вітрі не вижу»)
constexpr float kSwing = 4.0f;
// 3.0.20-beta.48 (the owner's «делай 1-9», 6; the audit's question 4): the swing is the game's pixels, sized for the
// top view - every tree some kUsualDepth from its narrow lens (kUsualFov degrees). From the eyes' level a tree at 30
// and a tree at 800 swung as many pixels: the far crowns tossed about. The swing of a tree `depth` from the eye under
// a lens of `fov` degrees - as far in the world as the top view's; at most kMostSwing times its pixels (a crown at
// the eye, a depth of nothing). The renderers take it in the 3D view alone: the top view's as it ever was
constexpr float kUsualDepth = 350.0f, kUsualFov = 15.0f, kMostSwing = 3.0f;
float swingAt(float swing, float depth, float fov);
// 3.0.20-beta.51 (the owner's «делай 1-9», 4 - denser crowns in the 3D view): a crown's leaves drawn kCrownCopies
// times more, each turned about the crown's middle's upright line and a little smaller - the flat cards' edges and
// gaps, seen from the eyes' level, closed. A copy's turn: its cosine, its sine, its size (leaf_vs: r9, r10, r11; a
// size of 0: the leaves as they are)
constexpr int kCrownCopies = 2;
void crownTurn(int copy, float out[3]);
// a place turned so about `middle` (up along -y): leaf_vs's sums
void turnedAbout(const float middle[3], const float turn[3], float p[3]);
// 3.0.20-beta.73 (the owner on the 3D view: «деревья очень примитивные ... это не деревья, можно что-то сделать?»): a
// tree's leaves are flat plates in tiers, made for the view from above; from the eyes' level they are seen edge-on,
// the sky between them. Each leaf face drawn again, stood up - turned a right angle about the line from the crown's
// middle's upright line through the face's own middle, laid flat - kStoodSize of its size about its middle: the
// tiers get a body. Tried on the game's own trees from the eyes' level and from above:
// /root/uagfx-tools/audit/trees/crowns2.py (across the line too: spikes - left out). 3.0.20-beta.79 (the owner's
// verdict on the trial: «доработать»): beta.73 drew two copies a face, one up and one down - both in the same plane,
// the second the first's mirror over it: every fin twice as thick, the crown a wall of leaves (his beta.77 shot of
// prt_fild06). Now kCrownStood copy, up or down by the face's own number (stoodWay): the same body, half the leaves.
// A stood copy's turn: its way up (1 or -1; 0 - by the face's number), nothing, its size under 0 (leaf_vs: r9, r10,
// r11 - a size under 0 tells a stood copy from a turned one)
constexpr int kCrownStood = 1;
constexpr float kStoodSize = 0.85f;
// only a flat plate gets its stood copies: its plane's upright share (its normal's, 0-1) this at least - a bush's
// upright leaf, a hanging frond stand already
constexpr float kStoodFlat = 0.6f;
void crownStood(int copy, float out[3]);
// the way a face's stood copy goes by its number among the faces (leaf_vs: the instance's face): up for an even one
float stoodWay(uint32_t face);
// A leaf face's corners stood so (leaf_vs's sums); `face` its number (the way when the turn gives none). False, the
// corners as they are - no copy of this face: a face on the trunk's own line, one that stands already, a turn that is
// no stood copy's.
bool stoodAbout(const float middle[3], const float turn[3], float a[3], float b[3], float c[3], uint32_t face = 0);

// 3.0.20-beta.79 (the owner on the trial of the crowns: «доработать»; his beta.77 shot - every crown one even green):
// a crown's own light ([view3d] crown_light) - the light of a leaf's corner at `p` by its place in its crown's box in
// the world (`middle`, `half`: across and up; up along -y): darker inside the crown (kCrownInner at its middle) and
// under it (kCrownUnder at its bottom), the side towards the sun (`toSun`, of length 1) kCrownSun brighter and the far
// one darker, all times kCrownGain - the crown on the whole as bright as the game's (offline:
// /root/uagfx-tools/audit/trees3 lit2.py, scene.py); kCrownLeast-kCrownMost. A crown of no size: 1. leaf_vs's
// crownLight holds the same
constexpr float kCrownInner = 0.68f, kCrownUnder = 0.7f, kCrownSun = 0.25f, kCrownGain = 1.22f;
constexpr float kCrownLeast = 0.25f, kCrownMost = 1.3f;
// a crown's box this much across and up at least (half sizes) to be lit so - a flat plate of leaves is not
constexpr float kCrownLeastHalf = 0.5f;
float crownLight(const float p[3], const float middle[3], const float half[2], const float toSun[3]);

}  // namespace uagfx::foliage
