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

}  // namespace uagfx::foliage
