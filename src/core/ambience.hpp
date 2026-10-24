#pragma once
#include <cstddef>
#include <vector>

#include "core/rsw.hpp"

namespace uagfx::ambience {

// 3.0.4-beta.7 (the user's idea 6): the map's own sounds around the character (core/rsw.hpp Sound: birds, brooks,
// waves, the wind, insects, caves' drops - 25,949 on 563 maps, uaRO.exe loads them and never plays them), played by
// the driver's engine (win/sound_engine.cpp). Places in the world's units (ten a ground tile), as the game's camera
// has them (the maps' axes x +1, z +1).
constexpr int kMost = 8;  // sounds at once

struct Heard {
    size_t source = 0;    // its index in the map's sounds
    float gain = 0.0f;    // its volume times the distance's
    float azimuth = 0.0f; // degrees from straight ahead, + right
    float distance = 0.0f;
};

// A sound at `distance` from the character: its volume, whole within a quarter of its reach, none past it
float gainAt(const rsw::Sound& s, float distance);
// The sounds the character at (x, y, z) hears, the screen's right (rx, rz) and its ahead (fx, fz): of each file the
// nearest within its reach (a shore's twenty waves sound once), the loudest `most` of them, loudest first
std::vector<Heard> heard(const std::vector<rsw::Sound>& sounds, float x, float y, float z, float rx, float rz, float fx,
                         float fz, int most = kMost);

}  // namespace uagfx::ambience
