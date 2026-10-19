#include "core/torch.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::torch {
namespace {

uint32_t mix(uint32_t v) {
    v ^= v >> 16;
    v *= 0x7FEB352Du;
    v ^= v >> 15;
    v *= 0x846CA68Bu;
    v ^= v >> 16;
    return v;
}

// Smooth value noise in [0, 1].
float noise(double x, uint32_t seed) {
    const double cell = std::floor(x);
    const float f = float(x - cell);
    const uint32_t i = uint32_t(int64_t(cell));
    const float a = float(mix(i * 0x9E3779B1u ^ mix(seed)) & 0xFFFFFFu) / float(0xFFFFFF);
    const float b = float(mix((i + 1) * 0x9E3779B1u ^ mix(seed)) & 0xFFFFFFu) / float(0xFFFFFF);
    const float u = f * f * (3.0f - 2.0f * f);
    return a + (b - a) * u;
}

}  // namespace

bool flameOf(const Corner q[4], Flame& out) {
    // The frame in the texture: what the corners span.
    double u0 = q[0].u, u1 = q[0].u, v0 = q[0].v, v1 = q[0].v;
    for (int i = 1; i < 4; ++i) {
        u0 = std::min(u0, double(q[i].u));
        u1 = std::max(u1, double(q[i].u));
        v0 = std::min(v0, double(q[i].v));
        v1 = std::max(v1, double(q[i].v));
    }
    if (!(u1 - u0 > 1e-6) || !(v1 - v0 > 1e-6)) return false;
    // The affine map from the texture's coordinates to the screen, from the three corners that span the most.
    static const int kTriples[4][3] = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}};
    const Corner *p0 = nullptr, *p1 = nullptr, *p2 = nullptr;
    double det = 0.0;
    for (const auto& t : kTriples) {
        const Corner &a = q[t[0]], &b = q[t[1]], &c = q[t[2]];
        const double d = (double(b.u) - a.u) * (double(c.v) - a.v) - (double(c.u) - a.u) * (double(b.v) - a.v);
        if (std::fabs(d) > std::fabs(det)) {
            det = d;
            p0 = &a;
            p1 = &b;
            p2 = &c;
        }
    }
    if (std::fabs(det) < 1e-3 * (u1 - u0) * (v1 - v0)) return false;  // the corners in a line
    const double s1 = double(p1->u) - p0->u, t1 = double(p1->v) - p0->v;
    const double s2 = double(p2->u) - p0->u, t2 = double(p2->v) - p0->v;
    const double x1 = double(p1->x) - p0->x, y1 = double(p1->y) - p0->y;
    const double x2 = double(p2->x) - p0->x, y2 = double(p2->y) - p0->y;
    const double ax = (x1 * t2 - x2 * t1) / det, bx = (x2 * s1 - x1 * s2) / det;  // x = x0 + ax du + bx dv
    const double ay = (y1 * t2 - y2 * t1) / det, by = (y2 * s1 - y1 * s2) / det;
    const double fu = (u1 - u0) / kFrameSide, fv = (v1 - v0) / kFrameSide;  // a texel of the frame
    const double bu = u0 + kBaseX * fu - p0->u, bv = v0 + kBaseY * fv - p0->v;
    out.baseX = float(p0->x + ax * bu + bx * bv);
    out.baseY = float(p0->y + ay * bu + by * bv);
    out.upX = float(-bx * kHeight * fv);
    out.upY = float(-by * kHeight * fv);
    out.halfWidth = float(std::hypot(ax * kHalfWidth * fu, ay * kHalfWidth * fu));
    out.z = (q[0].z + q[1].z + q[2].z + q[3].z) * 0.25f;
    out.rhw = (q[0].rhw + q[1].rhw + q[2].rhw + q[3].rhw) * 0.25f;
    return std::isfinite(out.baseX) && std::isfinite(out.baseY) && out.halfWidth > 0.0f;
}

bool flameOfQuad(const uint8_t* vertices, size_t stride, Flame& out) {
    Corner q[4];
    uint32_t diffuse = 0;
    for (int i = 0; i < 4; ++i) {
        const uint8_t* p = vertices + size_t(i) * stride;
        float xyzw[4], uv[2];
        std::memcpy(xyzw, p, sizeof xyzw);
        std::memcpy(uv, p + 24, sizeof uv);  // after the position, the diffuse and the specular colours
        q[i] = Corner{xyzw[0], xyzw[1], xyzw[2], xyzw[3], uv[0], uv[1]};
        if (i == 0) std::memcpy(&diffuse, p + 16, sizeof diffuse);
    }
    if (!flameOf(q, out) || out.rhw <= 0.0f) return false;
    out.alpha = std::min(1.0f, float(diffuse >> 24) / 240.0f);  // the .act shows the flame at 240
    return true;
}

float flicker(double t, uint32_t seed, float amount) {
    if (amount <= 0.0f) return 1.0f;
    const float n = 0.5f * noise(t * 7.0, seed) + 0.3f * noise(t * 13.0, seed + 1) + 0.2f * noise(t * 29.0, seed + 2);
    return 1.0f + amount * (2.0f * n - 1.0f);
}

uint32_t seedAt(float x, float z) {
    const int32_t i = int32_t(std::floor(x / 5.0f)), j = int32_t(std::floor(z / 5.0f));
    return mix(uint32_t(i) * 0x9E3779B1u ^ mix(uint32_t(j) + 0x632BE5ABu));
}

}  // namespace uagfx::torch
