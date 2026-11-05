#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::chime {

// 3.0.11-beta.129 (the owner «делай»: a sound when 10 arrows are left and when none are): the interface's short soft
// chimes, made here (no file of the game's fits) - falling bell notes, a quick rise and a slow fall each, the last
// faded to silence; a RIFF WAVE image (22050 Hz, 16-bit, mono) the game's own sound player takes as it takes its
// files (win/sound_hook playOwn). No Windows here.
enum class Kind : uint8_t {
    AmmoLow,  // 10 or fewer left: two notes, C6 then G5
    AmmoOut,  // none left: three, C6 G5 C5, the last longer
    PartyLow  // 3.0.11-beta.282: one of the party under 30 % HP - E6 E6 G6, quick
};
std::vector<uint8_t> wav(Kind kind);

}  // namespace uagfx::chime
