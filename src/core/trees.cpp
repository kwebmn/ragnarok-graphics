#include "core/trees.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>

namespace uagfx::trees {
namespace {

bool startsWith(const std::string& s, const char* p) { return s.compare(0, std::char_traits<char>::length(p), p) == 0; }

float hash(int x, int z, uint32_t salt) {
    uint32_t h = uint32_t(x) * 0x8da6b343u ^ uint32_t(z) * 0xd8163841u ^ salt * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return float(h & 0xffffff) / float(0xffffff);
}

// Value noise 0-1, smooth between the corners of a grid of `cell` units.
float noise(float x, float z, float cell, uint32_t salt) {
    const float fx = x / cell, fz = z / cell;
    const int ix = int(std::floor(fx)), iz = int(std::floor(fz));
    float tx = fx - float(ix), tz = fz - float(iz);
    tx = tx * tx * (3.0f - 2.0f * tx);
    tz = tz * tz * (3.0f - 2.0f * tz);
    const float a = hash(ix, iz, salt), b = hash(ix + 1, iz, salt), c = hash(ix, iz + 1, salt),
                d = hash(ix + 1, iz + 1, salt);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * tz;
}

}  // namespace

Season seasonOf(const std::string& map, int month) {
    if (startsWith(map, "xmas")) return Season::Rime;
    if (month < 9 || month > 11) return Season::Green;
    static const char* const kEvergreen[] = {"moc", "morocc", "ve_", "veins", "cmd", "comodo", "um_", "umbala",
                                             "bra", "dew", "mal", "ayo", "nif", "gl_", "glast", "ein", "lhz"};
    for (const char* p : kEvergreen)
        if (startsWith(map, p)) return Season::Green;
    return Season::Autumn;
}

int monthNow() {
    const std::time_t now = std::time(nullptr);
    const std::tm* t = std::localtime(&now);
    return t ? t->tm_mon + 1 : 1;
}

Shade shadeAt(float x, float z, Season season) {
    Shade s;
    const float light = noise(x, z, 14.0f, 1), hue = noise(x, z, 14.0f, 2), turn = noise(x, z, 11.0f, 3);
    const float b = 0.9f + 0.2f * light;
    // Towards yellow-green or towards blue-green, a little.
    const float warm = hue - 0.5f;
    s.tint[0] = b * (1.0f + 0.10f * warm);
    s.tint[1] = b * (1.0f + 0.03f * std::fabs(warm));
    s.tint[2] = b * (1.0f - 0.14f * warm);
    if (season == Season::Autumn && turn > 0.5f) {
        // A third of the trees golden, the most turned of them orange and red.
        static const float kGold[3] = {1.30f, 1.08f, 0.38f}, kRed[3] = {1.45f, 0.62f, 0.28f};
        const float k = std::clamp((turn - 0.62f) / 0.2f, 0.0f, 1.0f);
        for (int c = 0; c < 3; ++c) s.autumn[c] = kGold[c] + (kRed[c] - kGold[c]) * k;
        s.autumnAmount = std::clamp((turn - 0.5f) / 0.08f, 0.0f, 1.0f) * 0.7f;
    }
    if (season == Season::Rime) s.rime = 0.45f + 0.25f * light;
    return s;
}

float throughLight(float shadowLength, float night) {
    const float low = std::clamp((shadowLength - 0.55f) / 1.05f, 0.0f, 1.0f);  // 0.55 at noon, 1.6 at sunset
    return (0.45f + 0.55f * low) * std::clamp(1.0f - night * 1.5f, 0.0f, 1.0f);
}

}  // namespace uagfx::trees
