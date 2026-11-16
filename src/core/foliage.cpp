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
    // 3.0.11-beta.300 (the players: «в пронтере лавочка вместе с травой движется»): and a leaf's green is a yellow one
    // - Prontera's wrought bench (외부소품\pron-bench1-3.bmp: R 20-28, B 32-44) is painted a blue green; the client's
    // 45 leaves' textures have blue at most 2 over red (867 of its trees' and props' textures looked at)
    return hole > 0.12 && hole < 0.85 && mg > mr * 1.08 && mg > mb * 1.15 && mg > 40.0 && mb <= mr + 8.0;
}

int holesPercent(const uint32_t* argb, int width, int height) {
    if (!argb || width <= 0 || height <= 0) return 0;
    const size_t n = size_t(width) * size_t(height);
    size_t holes = 0;
    for (size_t i = 0; i < n; ++i) holes += (argb[i] >> 24) < 128;
    return int((holes * 100 + n / 2) / n);
}

bool leafyTexture(const uint32_t* argb, int width, int height, bool spriteCache) {
    return !spriteCache && leafy(argb, width, height);
}

float swingAt(float swing, float depth, float fov) {
    const float half = 3.14159265f / 360.0f;  // half a lens, radians a degree
    const float lens = std::tan(kUsualFov * half) / std::tan(std::clamp(fov, 1.0f, 170.0f) * half);
    const float k = depth > 1.0f ? kUsualDepth / depth * lens : kMostSwing;
    return swing * std::min(k, kMostSwing);
}

void crownTurn(int copy, float out[3]) {
    static const float kTurns[kCrownCopies][3] = {{0.5f, 0.8660254f, 0.94f}, {-0.5f, 0.8660254f, 0.88f}};  // 60, 120
    const int k = std::clamp(copy, 0, kCrownCopies - 1);
    for (int a = 0; a < 3; ++a) out[a] = kTurns[k][a];
}

void turnedAbout(const float middle[3], const float turn[3], float p[3]) {
    if (!(turn[2] > 0.0f)) return;
    const float d[3] = {(p[0] - middle[0]) * turn[2], (p[1] - middle[1]) * turn[2], (p[2] - middle[2]) * turn[2]};
    p[0] = middle[0] + d[0] * turn[0] - d[2] * turn[1];
    p[1] = middle[1] + d[1];
    p[2] = middle[2] + d[0] * turn[1] + d[2] * turn[0];
}

}  // namespace uagfx::foliage
