#pragma once
#include <cstdint>

namespace uagfx::motion {

// 3.0.11-beta.46 (the concept's «Движение»; the owner: «занимаемся доработками интерфейса»): the new interface's
// motion - short, eased out, all of it off by one switch ([interface] animations); what matters in a fight (HP, the
// cast, a cell's state) changes at once, without it. The times are the concept's.
constexpr uint64_t kCardMs = 120;    // the target's card shows and goes: its opacity and 4 px
constexpr uint64_t kBlockMs = 160;   // the character block's view changes: the old one out, the new in
constexpr uint64_t kToastInMs = 180, kToastOutMs = 240;  // a warning slides in from above, then fades
constexpr uint64_t kLevelMs = 600;   // a new level: the character block's gold edge flares once
constexpr float kCardShift = 4;      // px at scale 1

void setOn(bool on);  // [interface] animations (on by default)
bool on();

float easeOut(float t);  // 0..1 -> 0..1, cubic: quick, then settling
// A part shown at `since` (ms): its opacity at `now`, `ms` long; 1 with the motion off, or `since` 0
float fadeIn(uint64_t now, uint64_t since, uint64_t ms);
// A part gone at `since`: its opacity at `now` (0 past `ms`, and with the motion off)
float fadeOut(uint64_t now, uint64_t since, uint64_t ms);
// A flare at `since`: up in its first fifth, then down to 0 at `ms` - once (0 with the motion off)
float flare(uint64_t now, uint64_t since, uint64_t ms);

}  // namespace uagfx::motion
