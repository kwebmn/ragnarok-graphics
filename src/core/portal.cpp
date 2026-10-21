#include "core/portal.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::portal {
namespace {
float smooth(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}  // namespace

std::array<Ring, kRings> ringsAt(double t) {
    std::array<Ring, kRings> out{};
    const double newest = std::floor(t / double(kRingEvery));
    for (int k = 0; k < kRings; ++k) {
        const float age = float(t - (newest - k) * double(kRingEvery));
        if (age < 0.0f || age >= kRingLife) continue;
        const float f = age / kRingLife;
        out[size_t(k)].radius = kRingRadius - kRingShrink * age / kTick;
        out[size_t(k)].alpha = kRingAlpha * smooth(0.0f, 0.125f, f) * (1.0f - smooth(0.8f, 1.0f, f));
    }
    return out;
}

bool screenBox(const float foot[3], const float up[3], const camera::Projection& p, float height, float box[4]) {
    // two ways along the ground, square to the up
    const float pick[3] = {std::fabs(up[0]) < 0.9f ? 1.0f : 0.0f, std::fabs(up[0]) < 0.9f ? 0.0f : 1.0f, 0.0f};
    float a[3] = {up[1] * pick[2] - up[2] * pick[1], up[2] * pick[0] - up[0] * pick[2],
                  up[0] * pick[1] - up[1] * pick[0]};
    const float al = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    for (float& v : a) v /= al;
    const float b[3] = {up[1] * a[2] - up[2] * a[1], up[2] * a[0] - up[0] * a[2], up[0] * a[1] - up[1] * a[0]};
    const float r = kColumnRadius * 1.2f;
    box[0] = box[1] = 1e30f;
    box[2] = box[3] = -1e30f;
    for (int k = 0; k < 8; ++k)
        for (const float h : {0.0f, height}) {
            const float c = std::cos(float(k) * 0.785398f) * r, s = std::sin(float(k) * 0.785398f) * r;
            float q[3];
            for (int i = 0; i < 3; ++i) q[i] = foot[i] + a[i] * c + b[i] * s + up[i] * h;
            if (q[2] <= p.nearZ) return false;
            const float x = q[0] / q[2] * p.f0 + p.cx, y = q[1] / q[2] * p.f1 + p.cy;
            box[0] = std::min(box[0], x);
            box[1] = std::min(box[1], y);
            box[2] = std::max(box[2], x);
            box[3] = std::max(box[3], y);
        }
    return true;
}

}  // namespace uagfx::portal
