#include "core/model_fade.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::modelfade {

namespace {
struct V {
    float x, y, rhw;
};
V at(const uint8_t* v, uint32_t stride, uint32_t i) {
    float f[4];
    std::memcpy(f, v + size_t(i) * stride, sizeof f);
    return V{f[0], f[1], f[3]};
}
// the point's depth inside the triangle (rhw linear on the screen), or <= 0 when outside
float depthIn(const V& a, const V& b, const V& c, float px, float py) {
    const float d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (std::fabs(d) < 1e-6f) return 0.0f;
    const float l0 = ((b.y - c.y) * (px - c.x) + (c.x - b.x) * (py - c.y)) / d;
    const float l1 = ((c.y - a.y) * (px - c.x) + (a.x - c.x) * (py - c.y)) / d;
    const float l2 = 1.0f - l0 - l1;
    if (l0 < 0 || l1 < 0 || l2 < 0) return 0.0f;
    const float rhw = l0 * a.rhw + l1 * b.rhw + l2 * c.rhw;
    return rhw > 0 ? 1.0f / rhw : 0.0f;
}
}  // namespace

Focus focusOf(const camera::Projection& p, const float view[12], const float at[3]) {
    Focus f;
    float c[3];
    for (int j = 0; j < 3; ++j) c[j] = at[0] * view[j] + at[1] * view[3 + j] + at[2] * view[6 + j] + view[9 + j];
    if (!(c[2] > p.nearZ) || !std::isfinite(c[2])) return f;
    f.x = c[0] / c[2] * p.f0 + p.cx;
    f.y = c[1] / c[2] * p.f1 + p.cy;
    f.z = c[2];
    f.h = std::fabs(p.f1) * kBodyUnits / c[2];
    f.known = std::isfinite(f.x) && std::isfinite(f.y) && f.h > 0;
    return f;
}

bool covers(const uint8_t* vertices, uint32_t stride, uint32_t vertexCount, const uint16_t* indices,
            uint32_t indexCount, bool strip, const Focus& f) {
    if (!f.known || !vertices || stride < 16 || vertexCount < 3) return false;
    const float px[3] = {f.x, f.x, f.x}, py[3] = {f.y - f.h * 0.3f, f.y - f.h * 0.6f, f.y - f.h * 0.9f};
    const uint32_t n = indices ? indexCount : vertexCount;
    const uint32_t triangles = strip ? (n >= 3 ? n - 2 : 0) : n / 3;
    const auto index = [&](uint32_t k) { return indices ? uint32_t(indices[k]) : k; };
    for (uint32_t t = 0; t < triangles; ++t) {
        const uint32_t base = strip ? t : t * 3;
        const uint32_t i0 = index(base), i1 = index(base + 1), i2 = index(base + 2);
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) return false;
        const V a = at(vertices, stride, i0), b = at(vertices, stride, i1), c = at(vertices, stride, i2);
        for (int s = 0; s < 3; ++s) {
            const float z = depthIn(a, b, c, px[s], py[s]);
            if (z > 0 && z < f.z - kNearer) return true;
        }
    }
    return false;
}

uint64_t keyOf(const camera::Projection& p, const float view[12], const uint8_t* vertices, uint32_t vertexCount,
               uintptr_t texture) {
    if (!vertices || vertexCount == 0) return 0;
    const V v = at(vertices, 32, 0);
    float w[3] = {0, 0, 0};
    if (v.rhw > 0 && p.f0 != 0 && p.f1 != 0) {
        const float vz = 1.0f / v.rhw, c[3] = {(v.x - p.cx) * vz / p.f0, (v.y - p.cy) * vz / p.f1, vz};
        for (int i = 0; i < 3; ++i)  // the view's rows turned back: w = M (c - t)
            w[i] = view[3 * i] * (c[0] - view[9]) + view[3 * i + 1] * (c[1] - view[10]) +
                   view[3 * i + 2] * (c[2] - view[11]);
    }
    const auto q = [](float x) { return uint64_t(int64_t(std::lround(x / 8.0f)) & 0xFFFF); };
    return (uint64_t(texture) * 0x9E3779B97F4A7C15ull) ^ (q(w[0]) << 48) ^ (q(w[1]) << 32) ^ (q(w[2]) << 16) ^
           uint64_t(vertexCount);
}

float Fader::step(uint64_t key, bool covering, uint32_t frame) {
    auto it = alpha_.find(key);
    if (it == alpha_.end()) {
        if (!covering) return 1.0f;
        it = alpha_.emplace(key, Entry{}).first;
    }
    Entry& e = it->second;
    if (e.frame != frame) {  // once a frame, however many draws the model takes
        e.alpha = covering ? std::max(kSeen, e.alpha - kStep) : std::min(1.0f, e.alpha + kStep);
        e.frame = frame;
    }
    return e.alpha;
}

void Fader::frameDone(uint32_t frame) {
    for (auto it = alpha_.begin(); it != alpha_.end();) {
        const bool done = it->second.alpha >= 1.0f && it->second.frame != frame;
        it = done || frame - it->second.frame > 120 ? alpha_.erase(it) : std::next(it);
    }
}

}  // namespace uagfx::modelfade
