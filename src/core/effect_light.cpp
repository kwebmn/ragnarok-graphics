#include "core/effect_light.hpp"

#include <algorithm>

namespace uagfx::effect_light {

namespace {

// A picture's colour weighed by its pixels' alpha and brightness (an additive picture's black adds nothing): its
// mean, and how much of it shows (the weights a pixel, 0-1); false when nothing shows.
bool weighed(const uint32_t* argb, int width, int height, double mean[3], double& shown) {
    if (!argb || width <= 0 || height <= 0) return false;
    const int step = std::max(1, std::max(width, height) / 64);  // some 4 000 pixels of a big picture
    double sum[3] = {0, 0, 0}, weights = 0;
    long count = 0;
    for (int y = 0; y < height; y += step)
        for (int x = 0; x < width; x += step) {
            const uint32_t p = argb[size_t(y) * size_t(width) + size_t(x)];
            const double r = (p >> 16 & 255) / 255.0, g = (p >> 8 & 255) / 255.0, b = (p & 255) / 255.0;
            const double w = (p >> 24) / 255.0 * std::max({r, g, b});
            sum[0] += r * w;
            sum[1] += g * w;
            sum[2] += b * w;
            weights += w;
            ++count;
        }
    if (weights <= 1e-6 || count == 0) return false;
    for (int k = 0; k < 3; ++k) mean[k] = sum[k] / weights;
    shown = weights / double(count);
    return true;
}

}  // namespace

float fireOf(const uint32_t* argb, int width, int height, float colour[3]) {
    colour[0] = colour[1] = colour[2] = 0.0f;
    double m[3], shown = 0.0;
    if (!weighed(argb, width, height, m, shown)) return 0.0f;
    const double r = m[0], g = m[1], b = m[2];
    const double top = std::max({r, g, b});
    if (top <= 1e-6) return 0.0f;
    colour[0] = float(r / top);
    colour[1] = float(g / top);
    colour[2] = float(b / top);
    if (g > r * 1.05 || b > g) return 0.0f;                              // greenish, bluish, purple: not fire
    const double warm = std::clamp(((r - b) / r - 0.35) / 0.55, 0.0, 1.0);  // orange 1, yellow some, white none
    const double seen = std::clamp(shown / 0.04, 0.0, 1.0);              // a speck of fire in a big picture
    return float(warm * std::max(seen, 0.5));
}

float lightOf(const uint32_t* argb, int width, int height, float colour[3]) {
    colour[0] = colour[1] = colour[2] = 0.0f;
    double m[3], shown = 0.0;
    if (!weighed(argb, width, height, m, shown)) return 0.0f;
    const double top = std::max({m[0], m[1], m[2]}), low = std::min({m[0], m[1], m[2]});
    if (top < 0.25) return 0.0f;  // too dark to light anything
    for (int k = 0; k < 3; ++k) colour[k] = float(m[k] / top);
    const double seen = std::max(std::clamp(shown / 0.04, 0.0, 1.0), 0.5);
    const double saturation = (top - low) / top;
    if (saturation >= 0.25) return float(std::min(1.0, saturation) * seen);
    return top >= 0.85 ? float(0.35 * seen) : 0.0f;  // near white: a flash of lightning, holy light
}

}  // namespace uagfx::effect_light
