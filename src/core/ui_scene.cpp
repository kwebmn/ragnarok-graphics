#include "core/ui_scene.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::uiscene {
namespace {

constexpr float kPi = 3.14159265f;

struct V2 {
    float x, y;
};

// The rounded box's outline grown by `o` (shrunk when negative): `n` points a corner, clockwise from the top left's
// arc - the same count at every `o`, so that two outlines pair point by point
void outline(const Prim& p, float o, int n, std::vector<V2>& out) {
    out.clear();
    const float x0 = p.x - o, y0 = p.y - o, x1 = p.x + p.w + o, y1 = p.y + p.h + o;
    const float rr = std::max(0.0f, std::min(p.r, std::min(p.w, p.h) / 2) + o);
    const float cxs[4] = {x0 + rr, x1 - rr, x1 - rr, x0 + rr}, cys[4] = {y0 + rr, y0 + rr, y1 - rr, y1 - rr};
    for (int c = 0; c < 4; ++c) {
        const float a0 = kPi + float(c) * kPi / 2;  // from the left (top left), round clockwise on the screen
        for (int i = 0; i < n; ++i) {
            const float a = a0 + (kPi / 2) * float(i) / float(n - 1);
            out.push_back({cxs[c] + rr * std::cos(a), cys[c] + rr * std::sin(a)});
        }
    }
}

uint32_t premultiplied(uint32_t rgb, float a) {
    const float k = std::clamp(a, 0.0f, 1.0f);
    const auto ch = [&](int sh) { return uint32_t(std::lround(float((rgb >> sh) & 0xFF) * k)); };
    return uint32_t(std::lround(k * 255)) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
}

struct Emit {
    float dx, dy, k, half;
    std::vector<glint::Vertex>& out;
    void tri(const V2& a, uint32_t ca, const V2& b, uint32_t cb, const V2& c, uint32_t cc) {
        for (const auto& [p, col] : {std::pair{a, ca}, std::pair{b, cb}, std::pair{c, cc}}) {
            glint::Vertex v{};
            v.x = (p.x + dx - half) / k;
            v.y = (p.y + dy - half) / k;
            v.rhw = 1;
            v.diffuse = col;
            out.push_back(v);
        }
    }
    // the band between two paired outlines, each point its colour
    void band(const std::vector<V2>& a, uint32_t ca, const std::vector<V2>& b, uint32_t cb) {
        const size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; ++i) {
            const size_t j = (i + 1) % n;
            tri(a[i], ca, a[j], ca, b[i], cb);
            tri(a[j], ca, b[j], cb, b[i], cb);
        }
    }
    void fan(const std::vector<V2>& a, uint32_t c) {
        for (size_t i = 1; i + 1 < a.size(); ++i) tri(a[0], c, a[i], c, a[i + 1], c);
    }
};

}  // namespace

void mesh(const Prim& p, float dx, float dy, float k, float half, float opacity, std::vector<glint::Vertex>& out) {
    if (p.kind != Kind::Rect || p.w <= 0 || p.h <= 0 || k <= 0) return;
    const float rr = std::min(p.r, std::min(p.w, p.h) / 2);
    const int n = std::clamp(int(std::ceil(rr / 1.5f)) + 2, 2, 24);  // a corner's points: smooth at its size
    Emit e{dx, dy, k, half, out};
    std::vector<V2> outer, inner, a, b;
    if (p.fillA > 0) {  // the fill: whole inside, a pixel's fringe across its border (the painter's cover(d))
        const uint32_t c = premultiplied(p.fill, p.fillA * opacity);
        outline(p, 0.5f, n, outer);
        outline(p, -0.5f, n, inner);
        e.fan(inner, c);
        e.band(inner, c, outer, 0);
    }
    if (p.edgeA > 0 && p.edgeW > 0) {  // the ring inside the border, fringed on both sides
        const uint32_t c = premultiplied(p.edge, p.edgeA * opacity);
        outline(p, 0.5f, n, outer);
        outline(p, -0.5f, n, a);
        const float in = std::min(p.edgeW, std::min(p.w, p.h) / 2);
        outline(p, -std::max(0.5f, in - 0.5f), n, b);
        outline(p, -(in + 0.5f), n, inner);
        e.band(a, c, outer, 0);
        if (in > 1.0f) e.band(b, c, a, c);
        e.band(inner, 0, b, c);
    }
}

}  // namespace uagfx::uiscene
