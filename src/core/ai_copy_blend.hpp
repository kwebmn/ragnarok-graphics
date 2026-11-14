#pragma once
#include <cstdint>

#include "core/d3d7.hpp"
#include "core/sprite_exe.hpp"

namespace uagfx::aicopy {

// 3.0.11-beta.197 (the tester: «візуальний баг ассумптіо + енжел вінгс на топ голові» - a white square round the
// head): an HD copy keeps its edge's colour under its see-through texels (its filtering takes no other colour in),
// where the game's own picture is black; a blended draw adding it with the source's factor one leaves its alpha unused
// - its colour then times its alpha (the shaders' alphaTest bit 23), as the game's own picture would add
inline bool colourTimesAlpha(bool aiCopy, bool blend, uint32_t srcBlend) {
    return aiCopy && blend && srcBlend == d3d7::BLEND_ONE;
}

// 3.0.19-beta.5 (R-P75FMG: «знову асумптіо квадратом навколо персонажа»): under Assumptio the game draws the
// character's body and head a second time, a little larger and pulsing, added as light (uaRO.exe 0x545b40: the actor's
// flag 8 at +0xD4, set by effect 375; its quad through 0x547e20 in mode 5 - source alpha, destination one). An actor's
// frame (a player's, a monster's, an NPC's: core/sprite_exe.hpp Source) added as light in the world takes the game's
// own picture, made for adding (black where see-through); the frame drawn as usual keeps its HD copy, and so does a
// skill's sprite (its own rules)
inline bool gameOwnWhenAdded(uint8_t source, bool ui, bool blend, uint32_t dstBlend) {
    using S = sprite_exe::Source;
    const bool actor = source == uint8_t(S::Player) || source == uint8_t(S::Monster) || source == uint8_t(S::Npc);
    return actor && !ui && blend && dstBlend == d3d7::BLEND_ONE;
}

}  // namespace uagfx::aicopy
