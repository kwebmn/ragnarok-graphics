#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace uagfx::glint {

// 0.95: a sparkle over a card lying on the ground (effects.glint) - a four-pointed star of two thin crossed bars,
// drawn adding light, that flares now and then: a short flash every kPeriod seconds, each card at its own moment.
constexpr double kPeriod = 1.8;

// How bright the star is at `seconds` for the card whose seed is `seed`: 0 most of the time, a quick rise and fall.
inline float flare(double seconds, uint32_t seed) {
    const double phase = std::fmod(seconds / kPeriod + double(seed % 997) / 997.0, 1.0);
    if (phase > 0.22) return 0.0f;
    return float(std::sin(phase / 0.22 * 3.14159265358979));
}

// A pre-transformed vertex as the game draws them (0x1C4: x y z rhw, diffuse, specular, u v).
struct Vertex {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u, v;
};
static_assert(sizeof(Vertex) == 32, "the game's vertex");

// The star's two bars as two four-vertex strips (8 vertices), centred on (x, y), `size` pixels from tip to tip, at
// the card's depth (z, rhw), white-gold at `strength` (0..1).
inline void star(float x, float y, float z, float rhw, float size, float strength, Vertex out[8]) {
    const float half = size * 0.5f, thin = std::fmax(size * 0.06f, 1.0f);
    const uint32_t a = uint32_t(std::fmin(std::fmax(strength, 0.0f), 1.0f) * 255.0f + 0.5f);
    const uint32_t colour = a << 24 | 0xFFF2CC;
    const float bars[2][4] = {{x - half, y - thin, x + half, y + thin}, {x - thin, y - half, x + thin, y + half}};
    for (int b = 0; b < 2; ++b) {
        const float* r = bars[b];
        const float corner[4][2] = {{r[0], r[1]}, {r[2], r[1]}, {r[0], r[3]}, {r[2], r[3]}};  // a strip's order
        for (int k = 0; k < 4; ++k) out[b * 4 + k] = Vertex{corner[k][0], corner[k][1], z, rhw, colour, 0, 0, 0};
    }
}

}  // namespace uagfx::glint
