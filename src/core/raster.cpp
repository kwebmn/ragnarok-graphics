#include "core/raster.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/d3d7.hpp"
#include "core/scale.hpp"

namespace uagfx {
namespace {

double edge(double ax, double ay, double bx, double by, double px, double py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

// With the triangle oriented so its area is positive (y down), top edges run right along a horizontal line and
// left edges go up; pixels exactly on them belong to this triangle.
bool topLeft(double ax, double ay, double bx, double by) {
    const double dx = bx - ax;
    const double dy = by - ay;
    return (dy == 0 && dx > 0) || dy < 0;
}

int wrapIndex(int i, int n, uint32_t address) {
    if (address == d3d7::TADDRESS_WRAP) return ((i % n) + n) % n;
    return std::clamp(i, 0, n - 1);  // CLAMP; BORDER and MIRROR are not needed by the tests
}

Rgb texel(const RefTexture& t, int x, int y, uint32_t address) {
    const uint32_t c = t.argb[size_t(wrapIndex(y, t.height, address)) * size_t(t.width) + size_t(wrapIndex(x, t.width, address))];
    return Rgb{double((c >> 16) & 255) / 255.0, double((c >> 8) & 255) / 255.0, double(c & 255) / 255.0};
}

Rgb sample(const RefTexture& t, double u, double v, bool linear, uint32_t address) {
    if (!linear) return texel(t, int(std::floor(u * t.width)), int(std::floor(v * t.height)), address);
    const double s = u * t.width - 0.5;
    const double q = v * t.height - 0.5;
    const int x0 = int(std::floor(s));
    const int y0 = int(std::floor(q));
    const double fx = s - x0;
    const double fy = q - y0;
    const Rgb a = texel(t, x0, y0, address), b = texel(t, x0 + 1, y0, address);
    const Rgb c = texel(t, x0, y0 + 1, address), d = texel(t, x0 + 1, y0 + 1, address);
    auto mix = [&](double pa, double pb, double pc, double pd) {
        return (pa * (1 - fx) + pb * fx) * (1 - fy) + (pc * (1 - fx) + pd * fx) * fy;
    };
    return Rgb{mix(a.r, b.r, c.r, d.r), mix(a.g, b.g, c.g, d.g), mix(a.b, b.b, c.b, d.b)};
}

}  // namespace

void tlToNdc(float x, float y, int width, int height, float& ndcX, float& ndcY) {
    ndcX = (x + 0.5f) * 2.0f / float(width) - 1.0f;
    ndcY = 1.0f - (y + 0.5f) * 2.0f / float(height);
}

RefRaster::RefRaster(int width, int height, PixelCenters centers, double scale)
    : w_(width), h_(height), centers_(centers), scale_(scale), px_(size_t(width) * size_t(height), Rgb{0, 0, 0}) {}

void RefRaster::clear(const Rgb& c) { std::fill(px_.begin(), px_.end(), c); }

void RefRaster::strip(const TlVertex* v, int count, const RefTexture& tex, bool linear, uint32_t address) {
    for (int i = 0; i + 2 < count; ++i) {
        if (i % 2 == 0)
            triangle(v[i], v[i + 1], v[i + 2], tex, linear, address);
        else
            triangle(v[i + 1], v[i], v[i + 2], tex, linear, address);
    }
}

void RefRaster::triangle(const TlVertex& a, const TlVertex& b, const TlVertex& c, const RefTexture& tex, bool linear,
                         uint32_t address) {
    TlVertex t[3] = {a, b, c};
    if (centers_ == PixelCenters::Ours)
        for (TlVertex& v : t) {
            v.x = float(targetCoord(v.x, scale_));
            v.y = float(targetCoord(v.y, scale_));
        }
    const TlVertex* p0 = &t[0];
    const TlVertex* p1 = &t[1];
    const TlVertex* p2 = &t[2];
    double area = edge(p0->x, p0->y, p1->x, p1->y, p2->x, p2->y);
    if (area == 0) return;
    if (area < 0) {
        std::swap(p1, p2);
        area = -area;
    }
    const double off = centers_ == PixelCenters::D3D7 ? 0.0 : 0.5;  // pixel (x, y) has its centre at (x+off, y+off)
    const double minX = std::min({double(p0->x), double(p1->x), double(p2->x)});
    const double maxX = std::max({double(p0->x), double(p1->x), double(p2->x)});
    const double minY = std::min({double(p0->y), double(p1->y), double(p2->y)});
    const double maxY = std::max({double(p0->y), double(p1->y), double(p2->y)});
    const int x0 = std::max(0, int(std::ceil(minX - off)));
    const int x1 = std::min(w_ - 1, int(std::floor(maxX - off)));
    const int y0 = std::max(0, int(std::ceil(minY - off)));
    const int y1 = std::min(h_ - 1, int(std::floor(maxY - off)));
    const bool tl12 = topLeft(p1->x, p1->y, p2->x, p2->y);
    const bool tl20 = topLeft(p2->x, p2->y, p0->x, p0->y);
    const bool tl01 = topLeft(p0->x, p0->y, p1->x, p1->y);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const double px = x + off;
            const double py = y + off;
            const double w0 = edge(p1->x, p1->y, p2->x, p2->y, px, py);
            const double w1 = edge(p2->x, p2->y, p0->x, p0->y, px, py);
            const double w2 = edge(p0->x, p0->y, p1->x, p1->y, px, py);
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            if ((w0 == 0 && !tl12) || (w1 == 0 && !tl20) || (w2 == 0 && !tl01)) continue;
            // Perspective-correct: interpolate attribute*rhw and rhw linearly, then divide.
            const double l0 = w0 / area, l1 = w1 / area, l2 = w2 / area;
            const double q = l0 * p0->rhw + l1 * p1->rhw + l2 * p2->rhw;
            const double u = (l0 * p0->u0 * p0->rhw + l1 * p1->u0 * p1->rhw + l2 * p2->u0 * p2->rhw) / q;
            const double v = (l0 * p0->v0 * p0->rhw + l1 * p1->v0 * p1->rhw + l2 * p2->v0 * p2->rhw) / q;
            const Rgb t = sample(tex, u, v, linear, address);
            const auto chan = [&](int shift) {
                const double d0 = double((p0->diffuse >> shift) & 255) / 255.0;
                const double d1 = double((p1->diffuse >> shift) & 255) / 255.0;
                const double d2 = double((p2->diffuse >> shift) & 255) / 255.0;
                return (l0 * d0 * p0->rhw + l1 * d1 * p1->rhw + l2 * d2 * p2->rhw) / q;
            };
            px_[size_t(y) * size_t(w_) + size_t(x)] = Rgb{t.r * chan(16), t.g * chan(8), t.b * chan(0)};
        }
    }
}

}  // namespace uagfx
