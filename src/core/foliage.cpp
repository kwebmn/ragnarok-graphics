#include "core/foliage.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::foliage {

bool leafy(const uint32_t* argb, int width, int height) {
    if (!argb || width < 8 || height < 8) return false;
    uint64_t holes = 0, solid = 0, r = 0, g = 0, b = 0;
    const size_t n = size_t(width) * size_t(height);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = argb[i];
        if ((p >> 24) < 128) {
            ++holes;
            continue;
        }
        ++solid;
        r += p >> 16 & 255;
        g += p >> 8 & 255;
        b += p & 255;
    }
    if (!solid) return false;
    const double hole = double(holes) / double(n);
    const double mr = double(r) / solid, mg = double(g) / solid, mb = double(b) / solid;
    // Leaves: holes between them (not a solid wall, not a lone small shape), green over red and blue.
    return hole > 0.12 && hole < 0.85 && mg > mr * 1.08 && mg > mb * 1.15 && mg > 40.0;
}

bool leafyTexture(const uint32_t* argb, int width, int height, bool spriteCache) {
    return !spriteCache && leafy(argb, width, height);
}

float sway(float x, float y, float bottom, float height, float amplitude, float t) {
    if (height <= 1.0f || amplitude <= 0.0f) return 0.0f;
    const float up = std::clamp((bottom - y) / height, 0.0f, 1.0f);
    const float wave = std::sin(t * 1.6f + x * 0.013f + y * 0.007f) * 0.7f + std::sin(t * 2.9f + x * 0.031f) * 0.3f;
    return amplitude * up * up * wave;  // the top swings, the trunk stays
}

}  // namespace uagfx::foliage
