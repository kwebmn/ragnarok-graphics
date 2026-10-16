#pragma once
#include <algorithm>
#include <cmath>

namespace uagfx::sound {

// 0.91: the game's sound (Miles Sound System, mss32.dll) through the driver's hooks of its imports (win/sound_hook):
// the digital driver at the music's own rate, the most voices the game's tables hold, quieter in the background and
// a limiter for crowds.

// The rate the digital driver is opened at: setting 0 leaves the game's (22050 Hz at most, setup.exe's choice).
inline unsigned rateFor(int setting, unsigned game) { return setting > 0 ? unsigned(setting) : game; }

// The limiter: up to kFree sounds at once play as the game set them; past that each is made quieter so that the sum
// stays about the loudness of kFree (the square root: uncorrelated sounds add up by their power), never below
// kFloor of the game's - a crowd's skills crackled as the mix clipped.
constexpr int kFree = 6;
constexpr double kFloor = 0.45;
inline double crowdGain(int playing) {
    if (playing <= kFree) return 1.0;
    return std::max(kFloor, std::sqrt(double(kFree) / double(playing)));
}

// A Miles volume (0-127) times a gain, rounded and kept in range.
inline int scaled(int volume, double gain) {
    return std::clamp(int(std::lround(double(volume) * gain)), 0, 127);
}

}  // namespace uagfx::sound
