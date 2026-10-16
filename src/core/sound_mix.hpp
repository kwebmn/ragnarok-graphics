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

// 0.92: left and right - a sound at (dx, dz) from the player (the world's x and z) with the screen's right at
// (rx, rz): Miles' pan 0 (left) - 64 (centre) - 127 (right), `strength` % of the way out; a sound at the player
// stays in the centre.
inline int panOf(float dx, float dz, float rx, float rz, int strength) {
    const double len = std::sqrt(double(dx) * dx + double(dz) * dz), r = std::sqrt(double(rx) * rx + double(rz) * rz);
    if (len < 1.0 || r < 1e-6) return 64;
    const double side = (double(dx) * rx + double(dz) * rz) / (len * r);  // -1 left .. 1 right
    return std::clamp(64 + int(std::lround(63.0 * side * std::clamp(strength, 0, 100) / 100.0)), 0, 127);
}

// 0.92: the music's change - the old piece out, the new in, over kFadeMs, at an even loudness (equal power).
constexpr double kFadeMs = 1500.0;
inline double fadeIn(double ms) {
    const double t = std::clamp(ms / kFadeMs, 0.0, 1.0);
    return std::sin(t * 1.5707963267948966);
}
inline double fadeOut(double ms) {
    const double t = std::clamp(ms / kFadeMs, 0.0, 1.0);
    return std::cos(t * 1.5707963267948966);
}

// 0.92: all the game's samples busy - the one to stop for a new sound: the quietest now (the farthest, or a crowd's
// least), -1 when there is none.
inline int quietest(const int* volumes, int count) {
    int best = -1;
    for (int i = 0; i < count; ++i)
        if (volumes[i] >= 0 && (best < 0 || volumes[i] < volumes[best])) best = i;
    return best;
}

}  // namespace uagfx::sound
