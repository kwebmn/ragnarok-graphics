#include "core/effect_light.hpp"

#include <algorithm>

namespace uagfx::effect_light {

float fireOf(const uint32_t* argb, int width, int height, float colour[3]) {
    colour[0] = colour[1] = colour[2] = 0.0f;
    if (!argb || width <= 0 || height <= 0) return 0.0f;
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
    if (weights <= 1e-6 || count == 0) return 0.0f;
    const double r = sum[0] / weights, g = sum[1] / weights, b = sum[2] / weights;
    const double top = std::max({r, g, b});
    if (top <= 1e-6) return 0.0f;
    colour[0] = float(r / top);
    colour[1] = float(g / top);
    colour[2] = float(b / top);
    if (g > r * 1.05 || b > g) return 0.0f;                              // greenish, bluish, purple: not fire
    const double warm = std::clamp(((r - b) / r - 0.35) / 0.55, 0.0, 1.0);  // orange 1, yellow some, white none
    const double seen = std::clamp(weights / double(count) / 0.04, 0.0, 1.0);  // a speck of fire in a big picture
    return float(warm * std::max(seen, 0.5));
}

}  // namespace uagfx::effect_light
