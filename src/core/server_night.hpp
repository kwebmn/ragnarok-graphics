#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::server_night {

// 2.6.13: the server's night - its day and night cycle sends the status SI_NIGHT (160) on the maps flagged
// nightenabled (prontera, the fields) - darkens the world's light in uaRO.exe to half, blue, in a second or two: its
// environment at 0x7710B8 has a first byte "dark" (set for that status, or a Soul Linker's spirit, 149), and the
// light's factors fade 0.005 a frame towards it. The time of day is the driver's own («Часы», «Ночь»):
// effects.no_server_night takes that darkness away when the night alone made it.
constexpr uint32_t kDark = 0x7710B8;                                  // 1: dark
constexpr uint32_t kStatusFirst = 0x728484, kStatusLast = 0x728488;  // the player's statuses, a std::vector<int>
constexpr int kNight = 160, kSpirit = 149;
// Where this build has them: FUN_005b8e70 clears the byte and walks the statuses (kStatusFirst, kStatusLast) ...
constexpr uint32_t kWalk = 0x5B8E7E;
constexpr size_t kWalkSize = 31;
extern const uint8_t kWalkBytes[kWalkSize];
// ... and darkens for the night
constexpr uint32_t kForNight = 0x5B9609;
constexpr size_t kForNightSize = 17;
extern const uint8_t kForNightBytes[kForNightSize];

// The dark byte this frame: `dark` as the game has it, the night's and the spirit's statuses on the player, whether
// the night is left out; `held` the driver's own note that it took the night's darkness away (given back when the
// night is no longer left out). With nothing held and nothing to take, the game's byte as it is.
uint8_t darkNow(uint8_t dark, bool night, bool spirit, bool leaveOut, bool& held);

}  // namespace uagfx::server_night
