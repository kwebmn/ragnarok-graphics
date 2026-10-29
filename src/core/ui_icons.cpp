#include "core/ui_icons.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace uagfx::uiicons {
namespace {

#include "core/ui_icons.inc"  // kIconPoints, kIconLines, kIconShapes - tools/ui_icons.py

static_assert(sizeof kIconShapes / sizeof kIconShapes[0] == size_t(Icon::kCount), "an icon for each Icon");

struct Segment {
    float ax, ay, dx, dy, len2;
};

}  // namespace

void draw(panel::Picture& p, Icon icon, float x, float y, float size, uint32_t rgb, float a, float stroke) {
    if (icon >= Icon::kCount || size <= 0 || a <= 0 || p.px.size() != size_t(p.width) * size_t(p.height)) return;
    const float k = size / 24.0f, half = std::max(0.5f, stroke * k / 2);
    std::vector<Segment> segments;
    float left = 1e9f, top = 1e9f, right = -1e9f, bottom = -1e9f;
    const auto& shape = kIconShapes[size_t(icon)];
    for (size_t l = shape.first; l < size_t(shape.first) + shape.count; ++l) {
        const IconLine& line = kIconLines[l];
        const size_t n = line.count, ends = line.closed ? n : n - 1;
        for (size_t i = 0; i < ends; ++i) {
            const float* p0 = kIconPoints[line.first + i];
            const float* p1 = kIconPoints[line.first + (i + 1) % n];
            const float ax = x + p0[0] * k, ay = y + p0[1] * k, bx = x + p1[0] * k, by = y + p1[1] * k;
            segments.push_back(Segment{ax, ay, bx - ax, by - ay, (bx - ax) * (bx - ax) + (by - ay) * (by - ay)});
            left = std::min({left, ax, bx});
            right = std::max({right, ax, bx});
            top = std::min({top, ay, by});
            bottom = std::max({bottom, ay, by});
        }
    }
    // one coverage a pixel, the nearest line's (the lines' joints not drawn twice): each line's piece over its own
    // little box only, the most of them kept, then the icon's pixels once
    const int x0 = std::max({0, p.clipX0, int(std::floor(left - half - 1))});
    const int y0 = std::max({0, p.clipY0, int(std::floor(top - half - 1))});
    const int x1 = std::min({p.width, p.clipX1, int(std::ceil(right + half + 1)) + 1});
    const int y1 = std::min({p.height, p.clipY1, int(std::ceil(bottom + half + 1)) + 1});
    if (x0 >= x1 || y0 >= y1) return;
    const int w = x1 - x0;
    std::vector<float> cover(size_t(w) * size_t(y1 - y0), 0.0f);
    for (const Segment& s : segments) {
        const float bx0 = std::min(s.ax, s.ax + s.dx), bx1 = std::max(s.ax, s.ax + s.dx);
        const float by0 = std::min(s.ay, s.ay + s.dy), by1 = std::max(s.ay, s.ay + s.dy);
        const int sx0 = std::max(x0, int(std::floor(bx0 - half - 1)));
        const int sx1 = std::min(x1, int(std::ceil(bx1 + half + 1)) + 1);
        const int sy0 = std::max(y0, int(std::floor(by0 - half - 1)));
        const int sy1 = std::min(y1, int(std::ceil(by1 + half + 1)) + 1);
        for (int py = sy0; py < sy1; ++py)
            for (int px = sx0; px < sx1; ++px) {
                const float ex = float(px) + 0.5f - s.ax, ey = float(py) + 0.5f - s.ay;
                const float t = s.len2 > 0 ? std::clamp((ex * s.dx + ey * s.dy) / s.len2, 0.0f, 1.0f) : 0.0f;
                const float fx = ex - t * s.dx, fy = ey - t * s.dy;
                const float c = std::clamp(half + 0.5f - std::sqrt(fx * fx + fy * fy), 0.0f, 1.0f);
                float& kept = cover[size_t(py - y0) * size_t(w) + size_t(px - x0)];
                kept = std::max(kept, c);
            }
    }
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px)
            if (const float c = cover[size_t(py - y0) * size_t(w) + size_t(px - x0)]; c > 0)
                panel::over(p.px[size_t(py) * size_t(p.width) + size_t(px)], rgb, a * c);
}

}  // namespace uagfx::uiicons
