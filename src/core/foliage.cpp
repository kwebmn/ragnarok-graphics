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

void crownStood(int, float out[3]) {
    out[0] = 0.0f;  // the way by the face's own number (stoodWay)
    out[1] = 0.0f;
    out[2] = -kStoodSize;
}

float stoodWay(uint32_t face) { return (face & 1u) != 0u ? -1.0f : 1.0f; }

float crownLight(const float p[3], const float middle[3], const float half[2], const float toSun[3]) {
    if (!(half[0] > kCrownLeastHalf) || !(half[1] > kCrownLeastHalf)) return 1.0f;
    const float d[3] = {(p[0] - middle[0]) / half[0], (p[1] - middle[1]) / half[1], (p[2] - middle[2]) / half[0]};
    const float rho = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    const float k = 1.0f / std::max(rho, 1e-4f);
    const float n[3] = {d[0] * k, d[1] * k, d[2] * k};
    const float inner = kCrownInner + (1.0f - kCrownInner) * std::clamp((rho - 0.1f) / 0.8f, 0.0f, 1.0f);
    const float under = kCrownUnder + (1.0f - kCrownUnder) * std::clamp(0.55f - 0.6f * n[1], 0.0f, 1.0f);
    const float sun = 1.0f + kCrownSun * (n[0] * toSun[0] + n[1] * toSun[1] + n[2] * toSun[2]);
    return std::clamp(inner * under * sun * kCrownGain, kCrownLeast, kCrownMost);
}

bool stoodAbout(const float middle[3], const float turn[3], float a[3], float b[3], float c[3], uint32_t face) {
    if (!(turn[2] < 0.0f)) return false;
    const float mid[3] = {(a[0] + b[0] + c[0]) / 3.0f, (a[1] + b[1] + c[1]) / 3.0f, (a[2] + b[2] + c[2]) / 3.0f};
    const float ox = mid[0] - middle[0], oz = mid[2] - middle[2];
    const float len = std::sqrt(ox * ox + oz * oz);
    if (len < 1e-3f) return false;
    // its plane: flat enough?
    const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
    const float nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (!(nl > 1e-9f) || std::fabs(n[1]) < kStoodFlat * nl) return false;
    const float axis[3] = {ox / len, 0.0f, oz / len};
    const float size = -turn[2], way = turn[0] != 0.0f ? turn[0] : stoodWay(face);
    for (float* p : {a, b, c}) {
        const float d[3] = {(p[0] - mid[0]) * size, (p[1] - mid[1]) * size, (p[2] - mid[2]) * size};
        const float along = axis[0] * d[0] + axis[1] * d[1] + axis[2] * d[2];
        const float cross[3] = {axis[1] * d[2] - axis[2] * d[1], axis[2] * d[0] - axis[0] * d[2],
                                axis[0] * d[1] - axis[1] * d[0]};
        for (int i = 0; i < 3; ++i) p[i] = mid[i] + axis[i] * along + cross[i] * way;
    }
    return true;
}

}  // namespace uagfx::foliage
