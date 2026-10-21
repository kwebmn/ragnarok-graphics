#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace uagfx::glint {

// 0.95: a sparkle over a card lying on the ground (effects.glint), drawn adding light, that flares now and then: a
// short flash every kPeriod seconds, each card at its own moment (2.7.14: a sparkle, the two crossed bars gone).
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

// 2.7.14 (the tester: «хрестиком белым, не красивый»): a sparkle instead of the cross - four long rays and four
// short diagonal ones, each a thin diamond bright at its middle and gone at its tips, over a soft round glow; one list
// of triangles (kSparkle vertices) centred on (x, y), `size` pixels from tip to tip, at the card's depth. 2.7.15 (on
// Prontera's bright paving a thin needle): the long rays 8 % of the size wide, a golden glow at 75 %.
constexpr int kSparkle = 48;
inline void sparkle(float x, float y, float z, float rhw, float size, float strength, Vertex out[kSparkle]) {
    const float s = std::fmin(std::fmax(strength, 0.0f), 1.0f), half = size * 0.5f;
    const float wide = std::fmax(size * 0.08f, 1.2f);
    const auto alpha = [&](float share) { return uint32_t(s * share * 255.0f + 0.5f) << 24; };
    int n = 0;
    const auto put = [&](float px, float py, uint32_t colour) { out[n++] = Vertex{px, py, z, rhw, colour, 0, 0, 0}; };
    const float kRays[4][4] = {{1.0f, 0.0f, 1.0f, 1.0f},  // way x, way y, length's share, brightness
                               {0.0f, 1.0f, 1.0f, 1.0f},
                               {0.70710678f, 0.70710678f, 0.55f, 0.6f},
                               {0.70710678f, -0.70710678f, 0.55f, 0.6f}};
    for (const auto& r : kRays) {
        const float len = half * r[2], w = wide * (r[3] < 1.0f ? 0.8f : 1.0f);
        const float tx = r[0] * len, ty = r[1] * len, sx = -r[1] * w, sy = r[0] * w;
        const uint32_t tip = alpha(0.0f) | 0xFFEDB8, mid = alpha(r[3]) | 0xFFEDB8;
        put(x + tx, y + ty, tip), put(x + sx, y + sy, mid), put(x - sx, y - sy, mid);
        put(x - tx, y - ty, tip), put(x - sx, y - sy, mid), put(x + sx, y + sy, mid);
    }
    const float glow = size * 0.12f;  // the soft glow: a fan of eight as a list
    for (int k = 0; k < 8; ++k) {
        const float a0 = float(k) * 0.78539816f, a1 = float(k + 1) * 0.78539816f;
        put(x, y, alpha(0.75f) | 0xFFD27A);
        put(x + std::cos(a0) * glow, y + std::sin(a0) * glow, alpha(0.0f) | 0xFFD27A);
        put(x + std::cos(a1) * glow, y + std::sin(a1) * glow, alpha(0.0f) | 0xFFD27A);
    }
}

}  // namespace uagfx::glint
