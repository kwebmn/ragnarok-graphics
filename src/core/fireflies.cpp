#include "core/fireflies.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::fireflies {
namespace {

float smooth(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

float blink(double time, float seed) {
    const double phase = time * (0.35 + 0.25 * double(seed)) + double(seed) * 7.31;
    const float x = float(phase - std::floor(phase));  // within a flash's cycle
    return 0.18f + 0.82f * smooth(0.0f, 0.06f, x) * (1.0f - smooth(0.10f, 0.40f, x));
}

map::Point wander(double time, float seed) {
    const double t = time * (0.18 + 0.1 * double(seed));
    const double a = double(seed) * 6.2831853;
    const float x = float(3.0 * std::sin(t + a) + std::sin(t * 2.3 + a * 3.0));
    const float z = float(3.0 * std::cos(t * 0.83 + a * 1.7) + std::cos(t * 1.9 + a * 2.0));
    const float y = float(-5.0 + 2.5 * std::sin(t * 1.3 + a * 0.7) + 0.8 * std::sin(t * 3.1 + a));
    return map::Point{x, y, z};
}

void colourOf(uint32_t diffuse, float colour[3]) {
    const float r = float(diffuse >> 16 & 255u), g = float(diffuse >> 8 & 255u), b = float(diffuse & 255u);
    const float top = std::max({r, g, b}), low = std::min({r, g, b});
    if (top < 16.0f || (top - low) / top < 0.25f) {
        colour[0] = 0.72f;
        colour[1] = 1.0f;
        colour[2] = 0.32f;
        return;
    }
    colour[0] = r / top;
    colour[1] = g / top;
    colour[2] = b / top;
}

bool isFirefly(int effect) { return effect == 45 || effect == 713 || effect == 714 || effect == 946; }

void lampColour(float x, float z, float colour[3]) {
    static const float kColours[5][3] = {
        {1.0f, 0.86f, 0.45f},  // warm yellow
        {0.82f, 1.0f, 0.45f},  // yellow-green
        {0.55f, 1.0f, 0.6f},   // green
        {1.0f, 0.7f, 0.4f},    // amber
        {0.6f, 1.0f, 0.9f},    // pale mint
    };
    // its place to the nearest unit, mixed (a 32-bit hash): one colour a firefly, neighbours apart
    uint32_t h = uint32_t(int32_t(std::lround(x))) * 0x9E3779B1u ^ uint32_t(int32_t(std::lround(z))) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    const float* c = kColours[h % 5u];
    std::copy(c, c + 3, colour);
}

}  // namespace uagfx::fireflies
