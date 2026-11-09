#include "core/uaui_mesh.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::uaui {
namespace {

struct P {  // a point being clipped: place, colour by channel, uv
    float x, y, c[4], u, v;
};

P toP(const Point& p, float dx, float dy, const float* m) {
    P o{};
    float x = p.x + dx, y = p.y + dy;
    if (m) {  // column-major: x' = m0 x + m4 y + m12, y' = m1 x + m5 y + m13, w = m3 x + m7 y + m15
        const float w = m[3] * x + m[7] * y + m[15];
        const float nx = m[0] * x + m[4] * y + m[12], ny = m[1] * x + m[5] * y + m[13];
        x = w != 0 ? nx / w : nx;
        y = w != 0 ? ny / w : ny;
    }
    o.x = x, o.y = y, o.u = p.u, o.v = p.v;
    for (int i = 0; i < 4; ++i) o.c[i] = float((p.argb >> (24 - 8 * i)) & 0xFFu);  // a r g b
    return o;
}

P lerp(const P& a, const P& b, float t) {
    P o{};
    o.x = a.x + (b.x - a.x) * t, o.y = a.y + (b.y - a.y) * t;
    o.u = a.u + (b.u - a.u) * t, o.v = a.v + (b.v - a.v) * t;
    for (int i = 0; i < 4; ++i) o.c[i] = a.c[i] + (b.c[i] - a.c[i]) * t;
    return o;
}

// Sutherland-Hodgman against one edge: `inside(p)` >= 0 keeps, the crossing at value 0
template <typename F>
void cutBy(std::vector<P>& poly, F side) {
    std::vector<P> out;
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const P& a = poly[i];
        const P& b = poly[(i + 1) % n];
        const float sa = side(a), sb = side(b);
        if (sa >= 0) out.push_back(a);
        if ((sa >= 0) != (sb >= 0)) out.push_back(lerp(a, b, sa / (sa - sb)));
    }
    poly.swap(out);
}

glint::Vertex toGame(const P& p, float k, float half) {
    glint::Vertex v{};
    v.x = (p.x - half) / k;
    v.y = (p.y - half) / k;
    v.z = 0;
    v.rhw = 1;
    uint32_t c = 0;
    for (int i = 0; i < 4; ++i)
        c |= uint32_t(std::clamp(std::lround(p.c[i]), 0L, 255L)) << (24 - 8 * i);
    v.diffuse = c;
    v.specular = 0;
    v.u = p.u;
    v.v = p.v;
    return v;
}

}  // namespace

void triangles(const std::vector<Point>& points, const std::vector<int>& indices, float dx, float dy, const float* m,
               const ClipBox* clip, float k, float half, std::vector<glint::Vertex>& out) {
    if (k <= 0) return;
    std::vector<P> poly;
    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        const int i0 = indices[t], i1 = indices[t + 1], i2 = indices[t + 2];
        const int n = int(points.size());
        if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= n || i1 >= n || i2 >= n) continue;
        poly = {toP(points[size_t(i0)], dx, dy, m), toP(points[size_t(i1)], dx, dy, m),
                toP(points[size_t(i2)], dx, dy, m)};
        if (clip) {
            bool inside = true;
            for (const P& p : poly)
                inside = inside && p.x >= clip->x0 && p.x <= clip->x1 && p.y >= clip->y0 && p.y <= clip->y1;
            if (!inside) {
                cutBy(poly, [&](const P& p) { return p.x - clip->x0; });
                if (!poly.empty()) cutBy(poly, [&](const P& p) { return clip->x1 - p.x; });
                if (!poly.empty()) cutBy(poly, [&](const P& p) { return p.y - clip->y0; });
                if (!poly.empty()) cutBy(poly, [&](const P& p) { return clip->y1 - p.y; });
                if (poly.size() < 3) continue;
            }
        }
        for (size_t i = 1; i + 1 < poly.size(); ++i) {  // a fan
            out.push_back(toGame(poly[0], k, half));
            out.push_back(toGame(poly[i], k, half));
            out.push_back(toGame(poly[i + 1], k, half));
        }
    }
}

std::u32string codepoints(std::string_view text) {
    std::u32string out;
    const auto* s = reinterpret_cast<const unsigned char*>(text.data());
    const size_t n = text.size();
    for (size_t i = 0; i < n;) {
        const unsigned char c = s[i];
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        char32_t cp = len == 1 ? c : len == 2 ? (c & 0x1F) : len == 3 ? (c & 0x0F) : (c & 0x07);
        bool ok = len > 0 && i + size_t(len) <= n;
        for (int j = 1; ok && j < len; ++j) {
            ok = (s[i + size_t(j)] & 0xC0) == 0x80;
            cp = (cp << 6) | (s[i + size_t(j)] & 0x3F);
        }
        if (!ok) {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        out.push_back(cp);
        i += size_t(len);
    }
    return out;
}

}  // namespace uagfx::uaui
