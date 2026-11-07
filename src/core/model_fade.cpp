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
    f.p = p;
    std::memcpy(f.view, view, sizeof f.view);
    float cam[3];  // the camera's place: w = M (c - t) at c = 0
    for (int i = 0; i < 3; ++i)
        cam[i] = -(view[3 * i] * view[9] + view[3 * i + 1] * view[10] + view[3 * i + 2] * view[11]);
    f.camX = cam[0];
    f.camZ = cam[2];
    f.ground = std::hypot(at[0] - cam[0], at[2] - cam[2]);
    f.known = std::isfinite(f.x) && std::isfinite(f.y) && f.h > 0 && std::isfinite(f.ground);
    return f;
}

bool covers(const uint8_t* vertices, uint32_t stride, uint32_t vertexCount, const uint16_t* indices,
            uint32_t indexCount, bool strip, const Focus& f) {
    if (!f.known || !vertices || stride < 16 || vertexCount < 3) return false;
    float py[kPoints];
    for (int s = 0; s < kPoints; ++s) py[s] = f.y - f.h * kBodyAt[s];
    const uint32_t n = indices ? indexCount : vertexCount;
    const uint32_t triangles = strip ? (n >= 3 ? n - 2 : 0) : n / 3;
    const auto index = [&](uint32_t k) { return indices ? uint32_t(indices[k]) : k; };
    for (uint32_t t = 0; t < triangles; ++t) {
        const uint32_t base = strip ? t : t * 3;
        const uint32_t i0 = index(base), i1 = index(base + 1), i2 = index(base + 2);
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) return false;
        const V a = at(vertices, stride, i0), b = at(vertices, stride, i1), c = at(vertices, stride, i2);
        for (int s = 0; s < kPoints; ++s) {
            const float z = depthIn(a, b, c, f.x, py[s]);
            if (!(z > 0 && z < f.z - kNearer)) continue;
            // 3.0.13-beta.20: the point back into the world - nearer the camera along the ground than the character
            const float v[3] = {(f.x - f.p.cx) * z / f.p.f0, (py[s] - f.p.cy) * z / f.p.f1, z};
            float w[3];
            for (int i = 0; i < 3; ++i)
                w[i] = f.view[3 * i] * (v[0] - f.view[9]) + f.view[3 * i + 1] * (v[1] - f.view[10]) +
                       f.view[3 * i + 2] * (v[2] - f.view[11]);
            if (std::hypot(w[0] - f.camX, w[2] - f.camZ) < f.ground - kNearer) return true;
        }
    }
    return false;
}

Model modelOf(const camera::Projection& p, const float view[12], const uint8_t* vertices, uint32_t vertexCount,
              uintptr_t texture) {
    Model m;
    if (!vertices || vertexCount == 0) return m;
    m.id = uint64_t(texture) * 0x9E3779B97F4A7C15ull;
    if (p.f0 == 0 || p.f1 == 0) return m;
    double sum[3] = {0, 0, 0};
    uint32_t n = 0;
    for (uint32_t k = 0; k < vertexCount; ++k) {
        const V v = at(vertices, 32, k);
        if (!(v.rhw > 0)) continue;
        const float vz = 1.0f / v.rhw, c[3] = {(v.x - p.cx) * vz / p.f0, (v.y - p.cy) * vz / p.f1, vz};
        for (int i = 0; i < 3; ++i)  // the view's rows turned back: w = M (c - t)
            sum[i] += view[3 * i] * (c[0] - view[9]) + view[3 * i + 1] * (c[1] - view[10]) +
                      view[3 * i + 2] * (c[2] - view[11]);
        ++n;
    }
    if (n)
        for (int i = 0; i < 3; ++i) m.w[i] = float(sum[i] / n);
    return m;
}

bool boxOf(const uint8_t* vertices, uint32_t stride, uint32_t vertexCount, Box& out) {
    if (!vertices || stride < 16 || vertexCount == 0) return false;
    out = Box{1e9f, 1e9f, -1e9f, -1e9f, 1e9f};
    float most = 0;
    for (uint32_t i = 0; i < vertexCount; ++i) {
        const V v = at(vertices, stride, i);
        out.x0 = std::min(out.x0, v.x);
        out.y0 = std::min(out.y0, v.y);
        out.x1 = std::max(out.x1, v.x);
        out.y1 = std::max(out.y1, v.y);
        most = std::max(most, v.rhw);
    }
    out.nearest = most > 0 ? 1.0f / most : 1e9f;
    return out.x1 > out.x0 && out.y1 > out.y0;
}

bool Fader::alongside(const Model& m, const Box& b, float z) const {
    if (b.nearest >= z - kNearer) return false;  // behind the character, or beside it
    for (const Model& o : last_)
        if (std::hypot(o.w[0] - m.w[0], o.w[2] - m.w[2]) < kHouse) return true;
    return false;
}

float Fader::step(const Model& m, bool covering, uint32_t frame) {
    Entry* e = nullptr;
    float best = kSameModel * kSameModel;
    for (Entry& o : alpha_) {
        if (o.m.id != m.id) continue;
        const float dx = o.m.w[0] - m.w[0], dy = o.m.w[1] - m.w[1], dz = o.m.w[2] - m.w[2];
        const float d = dx * dx + dy * dy + dz * dz;
        if (d <= best) best = d, e = &o;
    }
    if (!e) {
        if (!covering || alpha_.size() >= 256) return 1.0f;
        alpha_.push_back(Entry{m, 1.0f, 0, frame});
        ++made_;
        e = &alpha_.back();
    }
    if (e->frame != frame) {  // once a frame, however many draws the model takes
        if (covering) e->coveredAt = frame;
        const bool held = frame - e->coveredAt <= kHoldFrames;
        e->alpha = held ? std::max(kSeen, e->alpha - kStep) : std::min(1.0f, e->alpha + kStep);
        e->frame = frame;
        e->m = m;  // follows a moving model
    } else if (covering) {
        e->coveredAt = frame;
    }
    return e->alpha;
}

void Fader::frameDone(uint32_t frame) {
    last_.swap(now_);  // 3.0.13-beta.19: this frame's covering boxes asked about in the next
    now_.clear();
    alpha_.erase(std::remove_if(alpha_.begin(), alpha_.end(),
                                [&](const Entry& e) {
                                    return (e.alpha >= 1.0f && e.frame != frame) || frame - e.frame > 120;
                                }),
                 alpha_.end());
}

}  // namespace uagfx::modelfade
