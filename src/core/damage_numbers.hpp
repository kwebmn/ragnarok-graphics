#pragma once
#include <cstdint>
#include <string>

namespace uagfx::damagenum {

// 3.0.16-beta.2 (the owner: «делай все» - the damage numbers among the game's looks redrawn): [interface]
// damage_numbers. uaRO.exe shows a number over an actor by its message 0x58 (value, ARGB colour, kind), drawn by
// FUN_00557eb0 as a sprite a digit (이팩트\숫자.spr, at most 999999): kind 0x15 a hit, 0x16 a critical (larger, a star:
// FUN_00558100), 0x10 yellow, 0x72 a recovery (green HP, blue SP). The new ones: crisp words with a dark rim in the
// game's colour, rising and fading.
enum class Kind : uint8_t { Hit, Critical, Heal, Other };
Kind kindOf(int gameKind);
std::string textOf(int value, Kind k);  // «1 234», «+567»
constexpr uint32_t kLifeMs = 1100, kFadeMs = 350;
// Where in its life a number is: how far up (in body heights, 0 at the head), its opacity and size (1 its own)
struct Pose {
    float rise = 0, opacity = 1, size = 1;
};
Pose poseAt(uint32_t ageMs, Kind k);
uint32_t rgbOf(uint32_t gameArgb, Kind k);  // its colour: the game's (white hits a little warm), the critical orange

}  // namespace uagfx::damagenum
