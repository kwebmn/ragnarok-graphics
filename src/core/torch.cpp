#include "core/torch.hpp"

#include <algorithm>
#include <cmath>

#include "core/sprite_exe.hpp"

namespace uagfx::torch {
namespace {

Layer layerOf(uint8_t source) {
    switch (sprite_exe::Source(source)) {
        case sprite_exe::Source::TorchGlow: return Layer::Glow;
        case sprite_exe::Source::TorchFlame: return Layer::Flame;
        default: return Layer::None;
    }
}

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

void Cells::note(uint32_t texture, const SpriteCell* cells, uint32_t count) {
    if (!cells || !count) return;
    auto it = byTexture_.find(texture);
    for (uint32_t i = 0; i < count; ++i) {
        const SpriteCell& c = cells[i];
        if (it != byTexture_.end()) {
            std::vector<Cell>& v = it->second;
            v.erase(std::remove_if(v.begin(), v.end(), [&c](const Cell& o) { return o.rect == c.rect; }), v.end());
        }
        const Layer layer = layerOf(c.source);
        if (layer == Layer::None) continue;
        if (it == byTexture_.end()) it = byTexture_.emplace(texture, std::vector<Cell>{}).first;
        it->second.push_back(Cell{c.rect, layer});
    }
    if (it != byTexture_.end() && it->second.empty()) byTexture_.erase(it);
}

void Cells::forget(uint32_t texture) { byTexture_.erase(texture); }

Layer Cells::at(uint32_t texture, float x, float y, Rect* cell) const {
    const auto it = byTexture_.find(texture);
    if (it == byTexture_.end()) return Layer::None;
    for (const Cell& c : it->second)
        if (x >= float(c.rect.left) && x < float(c.rect.right) && y >= float(c.rect.top) && y < float(c.rect.bottom)) {
            if (cell) *cell = c.rect;
            return c.layer;
        }
    return Layer::None;
}

bool flameOf(const Corner q[4], int width, int height, const Rect& cell, Flame& out) {
    if (width <= 0 || height <= 0) return false;
    // The affine map from texels to the screen, from the three corners that span the most of the texture.
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
    det *= double(width) * double(height);  // in texels
    if (std::fabs(det) < 1e-3) return false;
    const double s1 = (double(p1->u) - p0->u) * width, t1 = (double(p1->v) - p0->v) * height;
    const double s2 = (double(p2->u) - p0->u) * width, t2 = (double(p2->v) - p0->v) * height;
    const double x1 = double(p1->x) - p0->x, y1 = double(p1->y) - p0->y;
    const double x2 = double(p2->x) - p0->x, y2 = double(p2->y) - p0->y;
    const double ax = (x1 * t2 - x2 * t1) / det, bx = (x2 * s1 - x1 * s2) / det;  // x = x0 + ax ds + bx dt
    const double ay = (y1 * t2 - y2 * t1) / det, by = (y2 * s1 - y1 * s2) / det;
    const double s0 = double(p0->u) * width, t0 = double(p0->v) * height;
    const double bs = cell.left + double(kBaseX) - s0, bt = cell.top + double(kBaseY) - t0;
    out.baseX = float(p0->x + ax * bs + bx * bt);
    out.baseY = float(p0->y + ay * bs + by * bt);
    out.upX = float(-bx * kHeight);
    out.upY = float(-by * kHeight);
    out.halfWidth = float(std::hypot(ax * kHalfWidth, ay * kHalfWidth));
    out.z = (q[0].z + q[1].z + q[2].z + q[3].z) * 0.25f;
    out.rhw = (q[0].rhw + q[1].rhw + q[2].rhw + q[3].rhw) * 0.25f;
    return std::isfinite(out.baseX) && std::isfinite(out.baseY) && out.halfWidth > 0.0f;
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
