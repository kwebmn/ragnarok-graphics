#include "core/uaui_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

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
        if (clip && clip->hull.size() >= 6) {  // 3.0.18-beta.61: the mask's outline, counter-clockwise (hull())
            const std::vector<float>& h = clip->hull;
            const size_t n = h.size() / 2;
            for (size_t e = 0; e < n && poly.size() >= 3; ++e) {
                const float ax = h[2 * e], ay = h[2 * e + 1], bx = h[2 * ((e + 1) % n)], by = h[2 * ((e + 1) % n) + 1];
                cutBy(poly, [&](const P& p) { return (bx - ax) * (p.y - ay) - (by - ay) * (p.x - ax); });
            }
            if (poly.size() < 3) continue;
        }
        for (size_t i = 1; i + 1 < poly.size(); ++i) {  // a fan
            out.push_back(toGame(poly[0], k, half));
            out.push_back(toGame(poly[i], k, half));
            out.push_back(toGame(poly[i + 1], k, half));
        }
    }
}

std::vector<float> hull(const std::vector<Point>& points, float dx, float dy, const float* m) {
    std::vector<std::pair<float, float>> v;
    v.reserve(points.size());
    for (const Point& p : points) {
        const P q = toP(p, dx, dy, m);
        v.emplace_back(q.x, q.y);
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    if (v.size() < 3) return {};
    const auto cross = [](const std::pair<float, float>& o, const std::pair<float, float>& a,
                          const std::pair<float, float>& b) {
        return (a.first - o.first) * (b.second - o.second) - (a.second - o.second) * (b.first - o.first);
    };
    std::vector<std::pair<float, float>> h(2 * v.size());
    size_t k = 0;
    for (size_t i = 0; i < v.size(); ++i) {  // Andrew's chain: the lower side, then the upper
        while (k >= 2 && cross(h[k - 2], h[k - 1], v[i]) <= 1e-6f) --k;
        h[k++] = v[i];
    }
    for (size_t i = v.size() - 1, low = k + 1; i-- > 0;) {
        while (k >= low && cross(h[k - 2], h[k - 1], v[i]) <= 1e-6f) --k;
        h[k++] = v[i];
    }
    h.resize(k - 1);
    std::vector<float> out;
    for (const auto& [x, y] : h) out.push_back(x), out.push_back(y);
    return out;
}

namespace {

float placeOf(const Gradient& g, float x, float y) {
    if (g.radial) {
        const float rx = g.x1 > 0 ? g.x1 : 1, ry = g.y1 > 0 ? g.y1 : 1;
        return std::hypot((x - g.x0) / rx, (y - g.y0) / ry);
    }
    const float dx = g.x1 - g.x0, dy = g.y1 - g.y0, l2 = dx * dx + dy * dy;
    return l2 > 0 ? ((x - g.x0) * dx + (y - g.y0) * dy) / l2 : 0;
}

void colourAt(const Gradient& g, float t, float c[4]) {  // premultiplied a r g b
    const auto ch = [](uint32_t argb, int i) { return float((argb >> (24 - 8 * i)) & 0xFFu); };
    if (g.stops.empty()) {
        for (int i = 0; i < 4; ++i) c[i] = 0;
        return;
    }
    if (g.repeating && g.stops.size() > 1) {
        const float a = g.stops.front().t, b = g.stops.back().t;
        if (b > a) t = a + std::fmod(std::fmod(t - a, b - a) + (b - a), b - a);
    }
    if (t <= g.stops.front().t) {
        for (int i = 0; i < 4; ++i) c[i] = ch(g.stops.front().argb, i);
        return;
    }
    for (size_t s = 1; s < g.stops.size(); ++s) {
        const Gradient::Stop &a = g.stops[s - 1], &b = g.stops[s];
        if (t <= b.t) {
            const float f = b.t > a.t ? (t - a.t) / (b.t - a.t) : 1;
            for (int i = 0; i < 4; ++i) c[i] = ch(a.argb, i) + (ch(b.argb, i) - ch(a.argb, i)) * f;
            return;
        }
    }
    for (int i = 0; i < 4; ++i) c[i] = ch(g.stops.back().argb, i);
}

}  // namespace

void shade(std::vector<Point>& points, std::vector<int>& indices, const Gradient& g) {
    std::vector<Point> outPts;
    std::vector<int> outIx;
    std::vector<std::vector<P>> polys, next;
    const int n = int(points.size());
    for (size_t tr = 0; tr + 2 < indices.size(); tr += 3) {
        const int i0 = indices[tr], i1 = indices[tr + 1], i2 = indices[tr + 2];
        if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= n || i1 >= n || i2 >= n) continue;
        polys = {{toP(points[size_t(i0)], 0, 0, nullptr), toP(points[size_t(i1)], 0, 0, nullptr),
                  toP(points[size_t(i2)], 0, 0, nullptr)}};
        if (g.radial) {  // 3.0.18-beta.63: small triangles, each corner coloured by its distance
            std::vector<std::vector<P>> small;
            const std::function<void(const P&, const P&, const P&, int)> split = [&](const P& a, const P& b,
                                                                                     const P& c, int depth) {
                const float ab = std::hypot(a.x - b.x, a.y - b.y), bc = std::hypot(b.x - c.x, b.y - c.y),
                            ca = std::hypot(c.x - a.x, c.y - a.y);
                if (depth >= 8 || std::max({ab, bc, ca}) <= 8) {
                    small.push_back({a, b, c});
                    return;
                }
                const P mab = lerp(a, b, 0.5f), mbc = lerp(b, c, 0.5f), mca = lerp(c, a, 0.5f);
                split(a, mab, mca, depth + 1);
                split(mab, b, mbc, depth + 1);
                split(mca, mbc, c, depth + 1);
                split(mab, mbc, mca, depth + 1);
            };
            split(polys[0][0], polys[0][1], polys[0][2], 0);
            polys.swap(small);
        } else if (!g.repeating)
            for (const Gradient::Stop& st : g.stops) {  // cut where a stop's line crosses it
                next.clear();
                for (const std::vector<P>& poly : polys) {
                    float lo = 1e30f, hi = -1e30f;
                    for (const P& p : poly) {
                        const float t = placeOf(g, p.x, p.y);
                        lo = std::min(lo, t), hi = std::max(hi, t);
                    }
                    if (st.t <= lo + 1e-5f || st.t >= hi - 1e-5f) {
                        next.push_back(poly);
                        continue;
                    }
                    std::vector<P> below = poly, above = poly;
                    cutBy(below, [&](const P& p) { return st.t - placeOf(g, p.x, p.y); });
                    cutBy(above, [&](const P& p) { return placeOf(g, p.x, p.y) - st.t; });
                    if (below.size() >= 3) next.push_back(std::move(below));
                    if (above.size() >= 3) next.push_back(std::move(above));
                }
                polys.swap(next);
            }
        for (const std::vector<P>& poly : polys) {
            const int base = int(outPts.size());
            for (const P& p : poly) {
                float c[4];
                colourAt(g, placeOf(g, p.x, p.y), c);
                uint32_t argb = 0;
                for (int i = 0; i < 4; ++i)
                    argb |= uint32_t(std::clamp(std::lround(c[i] * p.c[i] / 255.0f), 0L, 255L)) << (24 - 8 * i);
                outPts.push_back(Point{p.x, p.y, argb, p.u, p.v});
            }
            for (size_t i = 1; i + 1 < poly.size(); ++i)
                outIx.insert(outIx.end(), {base, base + int(i), base + int(i) + 1});
        }
    }
    points.swap(outPts);
    indices.swap(outIx);
}

void softRing(float x, float y, float w, float h, float r, float blur, uint32_t argb, std::vector<Point>& points,
              std::vector<int>& indices) {
    points.clear();
    indices.clear();
    if (w <= 0 || h <= 0 || blur <= 0) return;
    r = std::clamp(r, 0.0f, std::min(w, h) / 2);
    constexpr int kSteps = 8;  // a corner's
    const float cx[4] = {x + w - r, x + w - r, x + r, x + r}, cy[4] = {y + r, y + h - r, y + h - r, y + r};
    const float start[4] = {-1.5707963f, 0, 1.5707963f, 3.1415927f};  // top right, bottom right, ... clockwise
    for (int c = 0; c < 4; ++c)
        for (int i = 0; i <= kSteps; ++i) {
            const float a = start[c] + 1.5707963f * float(i) / kSteps, ca = std::cos(a), sa = std::sin(a);
            points.push_back(Point{cx[c] + ca * r, cy[c] + sa * r, argb});
            points.push_back(Point{cx[c] + ca * (r + blur), cy[c] + sa * (r + blur), 0});
        }
    const int m = int(points.size()) / 2;
    for (int i = 0; i < m; ++i) {
        const int j = (i + 1) % m;
        indices.insert(indices.end(), {2 * i, 2 * i + 1, 2 * j + 1, 2 * i, 2 * j + 1, 2 * j});
    }
}

void roundedBox(float x, float y, float w, float h, float r, uint32_t argb, std::vector<Point>& points,
                std::vector<int>& indices) {
    points.clear();
    indices.clear();
    if (w <= 0 || h <= 0) return;
    r = std::clamp(r, 0.0f, std::min(w, h) / 2);
    constexpr int kSteps = 8;
    const float cx[4] = {x + w - r, x + w - r, x + r, x + r}, cy[4] = {y + r, y + h - r, y + h - r, y + r};
    const float start[4] = {-1.5707963f, 0, 1.5707963f, 3.1415927f};
    points.push_back(Point{x + w / 2, y + h / 2, argb});
    for (int c = 0; c < 4; ++c)
        for (int i = 0; i <= kSteps; ++i) {
            const float a = start[c] + 1.5707963f * float(i) / kSteps;
            points.push_back(Point{cx[c] + std::cos(a) * r, cy[c] + std::sin(a) * r, argb});
        }
    const int n = int(points.size()) - 1;
    for (int i = 0; i < n; ++i) indices.insert(indices.end(), {0, 1 + i, 1 + (i + 1) % n});
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
