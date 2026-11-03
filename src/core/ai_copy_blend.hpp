#pragma once
#include <cstdint>

#include "core/d3d7.hpp"

namespace uagfx::aicopy {

// 3.0.11-beta.197 (the tester: «візуальний баг ассумптіо + енжел вінгс на топ голові» - a white square round the
// head): an HD copy keeps its edge's colour under its see-through texels (its filtering takes no other colour in),
// where the game's own picture is black; a blended draw adding it with the source's factor one leaves its alpha unused
// - its colour then times its alpha (the shaders' alphaTest bit 23), as the game's own picture would add
inline bool colourTimesAlpha(bool aiCopy, bool blend, uint32_t srcBlend) {
    return aiCopy && blend && srcBlend == d3d7::BLEND_ONE;
}

}  // namespace uagfx::aicopy
