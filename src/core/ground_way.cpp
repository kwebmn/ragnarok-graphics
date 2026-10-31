#include "core/ground_way.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::groundway {

bool Heights::at(float x, float y, float& out) const {
    if (w <= 0 || h <= 0 || corners.size() < size_t(w) * size_t(h) * 4) return false;
    if (!(x >= 0) || !(y >= 0) || x >= float(w) || y >= float(h)) return false;
    const int cx = std::min(int(x), w - 1), cy = std::min(int(y), h - 1);
    const float fx = x - float(cx), fy = y - float(cy);
    const float* c = &corners[(size_t(cy) * size_t(w) + size_t(cx)) * 4];  // south-west, south-east, north-west, -east
    out = c[0] * (1 - fx) * (1 - fy) + c[1] * fx * (1 - fy) + c[2] * (1 - fx) * fy + c[3] * fx * fy;
    return true;
}

bool parseGat(const std::string& bytes, Heights& out) {
    if (bytes.size() < 14 || bytes.compare(0, 4, "GRAT") != 0) return false;
    int32_t w = 0, h = 0;
    std::memcpy(&w, bytes.data() + 6, 4);
    std::memcpy(&h, bytes.data() + 10, 4);
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048 || bytes.size() < 14 + size_t(w) * size_t(h) * 20) return false;
    Heights g;
    g.w = w;
    g.h = h;
    g.corners.resize(size_t(w) * size_t(h) * 4);
    for (size_t i = 0; i < size_t(w) * size_t(h); ++i)
        std::memcpy(&g.corners[i * 4], bytes.data() + 14 + i * 20, 16);
    out = std::move(g);
    return true;
}

std::vector<Point> along(const std::vector<std::pair<int, int>>& cells, const Heights& g, int cellSize, float lift,
                         float sign) {
    std::vector<Point> out;
    if (cells.empty() || cellSize <= 0) return out;
    const auto put = [&](float x, float y) {  // a place in cells (a cell's middle at +0.5)
        float hgt = 0;
        if (!g.at(x, y, hgt)) return;
        Point p;
        p.x = (x - float(g.w / 2)) * float(cellSize);
        p.z = (y - float(g.h / 2)) * float(cellSize);
        p.y = sign * hgt - lift;  // over the ground: up along -y
        out.push_back(p);
    };
    put(float(cells[0].first) + 0.5f, float(cells[0].second) + 0.5f);
    for (size_t i = 1; i < cells.size(); ++i) {
        const float x0 = float(cells[i - 1].first) + 0.5f, y0 = float(cells[i - 1].second) + 0.5f;
        const float x1 = float(cells[i].first) + 0.5f, y1 = float(cells[i].second) + 0.5f;
        const int steps = std::max(1, int(std::ceil(std::hypot(x1 - x0, y1 - y0))));  // a cell at the most
        for (int k = 1; k <= steps; ++k) {
            const float t = float(k) / float(steps);
            put(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        }
    }
    return out;
}

namespace {

// A world point on the screen as the game places its own vertices, its depth as of `bias` units nearer along its ray
// (its place on the screen kept); false behind the near plane
bool placed(const float w[3], const float view[12], const camera::Projection& p, float bias, uint32_t argb,
            Vertex& v) {
    float c[3] = {w[0] * view[0] + w[1] * view[3] + w[2] * view[6] + view[9],
                  w[0] * view[1] + w[1] * view[4] + w[2] * view[7] + view[10],
                  w[0] * view[2] + w[1] * view[5] + w[2] * view[8] + view[11]};
    own_ground::Tl2 t{};
    if (!own_ground::placeView(p, c, t)) return false;
    const float len = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);  // nearer along the ray: its depth alone
    const float k = len > bias ? 1.0f - bias / len : 1.0f;
    for (float& q : c) q *= k;
    own_ground::Tl2 nearer{};
    if (!own_ground::placeView(p, c, nearer)) nearer = t;
    v = Vertex{t.x, t.y, nearer.z, t.rhw, argb, 0u, 0.0f, 0.0f};
    return true;
}

}  // namespace

size_t strip(const std::vector<Point>& points, const float view[12], const camera::Projection& p, float width,
             uint32_t argb, float bias, std::vector<Vertex>& out) {
    const size_t before = out.size();
    if (points.size() < 2) return 0;
    // each point's sideways along the ground: across its legs, the bends' joined (their middle, kept as wide)
    std::vector<float> sx(points.size()), sz(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        float dx = 0, dz = 0;
        for (const size_t j : {i == 0 ? i : i - 1, i}) {
            if (j + 1 >= points.size()) continue;
            const float lx = points[j + 1].x - points[j].x, lz = points[j + 1].z - points[j].z;
            const float len = std::hypot(lx, lz);
            if (len > 1e-4f) {
                dx += lx / len;
                dz += lz / len;
            }
        }
        const float len = std::hypot(dx, dz);
        if (len < 1e-4f) {
            sx[i] = i ? sx[i - 1] : 1.0f;
            sz[i] = i ? sz[i - 1] : 0.0f;
            continue;
        }
        sx[i] = -dz / len;
        sz[i] = dx / len;
    }
    const auto place = [&](const Point& q, float side, size_t i, Vertex& v) {
        const float w[3] = {q.x + sx[i] * side, q.y, q.z + sz[i] * side};
        return placed(w, view, p, bias, argb, v);
    };
    const float half = width / 2;
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        Vertex a{}, b{}, c{}, d{};
        if (!place(points[i], -half, i, a) || !place(points[i], half, i, b) || !place(points[i + 1], -half, i + 1, c) ||
            !place(points[i + 1], half, i + 1, d))
            continue;  // behind the near plane
        out.insert(out.end(), {a, b, c, b, d, c});
    }
    return out.size() - before;
}

namespace {

// The way's length up to each point along the ground (x, z)
std::vector<float> runOf(const std::vector<Point>& points) {
    std::vector<float> run(points.size(), 0.0f);
    for (size_t i = 1; i < points.size(); ++i)
        run[i] = run[i - 1] + std::hypot(points[i].x - points[i - 1].x, points[i].z - points[i - 1].z);
    return run;
}

// The place `s` along the way: its leg's end point (index) and how far into the leg (0..1)
void legAt(const std::vector<float>& run, float s, size_t& i, float& t) {
    s = std::clamp(s, 0.0f, run.back());
    i = std::min(size_t(std::upper_bound(run.begin(), run.end(), s) - run.begin()), run.size() - 1);
    const size_t a = i ? i - 1 : 0;
    const float len = run[i] - run[a];
    t = len > 1e-6f ? (s - run[a]) / len : 0.0f;
}

}  // namespace

std::vector<Print> prints(const std::vector<Point>& points, float stride, float apart) {
    std::vector<Print> out;
    if (points.size() < 2 || !(stride > 0)) return out;
    const std::vector<float> run = runOf(points);
    const auto at = [&](float s, float& x, float& z) {  // the place `s` along
        size_t i = 0;
        float t = 0;
        legAt(run, s, i, t);
        const size_t a = i ? i - 1 : 0;
        x = points[a].x + (points[i].x - points[a].x) * t;
        z = points[a].z + (points[i].z - points[a].z) * t;
    };
    const int n = int(std::floor(run.back() / stride + 1e-4f));
    for (int k = n; k >= 0; --k) {  // from the way's start on, each counted back from its end
        Print print;
        print.at = std::max(0.0f, run.back() - float(k) * stride);
        print.left = k % 2 == 0;
        const float s = print.at;
        float bx = 0, bz = 0, ax = 0, az = 0, x = 0, z = 0;
        at(s - 2.0f, bx, bz);  // its toes along the way about it (a bend turns them, not jerks them)
        at(s + 2.0f, ax, az);
        at(s, x, z);
        const float len = std::hypot(ax - bx, az - bz);
        if (len < 1e-4f) continue;
        print.fx = (ax - bx) / len;
        print.fz = (az - bz) / len;
        const float side = print.left ? apart : -apart;  // the left of (fx, fz) is (-fz, fx)
        print.x = x - print.fz * side;
        print.z = z + print.fx * side;
        out.push_back(print);
    }
    return out;
}

std::vector<Point> cut(const std::vector<Point>& points, float at) {
    if (points.size() < 2) return points;
    const std::vector<float> run = runOf(points);
    size_t i = 0;
    float t = 0;
    legAt(run, at, i, t);
    const size_t a = i ? i - 1 : 0;
    if (at >= run.back()) return {points.back()};
    std::vector<Point> out{Point{points[a].x + (points[i].x - points[a].x) * t,
                                 points[a].y + (points[i].y - points[a].y) * t,
                                 points[a].z + (points[i].z - points[a].z) * t}};
    out.insert(out.end(), points.begin() + std::ptrdiff_t(i), points.end());
    return out;
}

std::vector<Print> ahead(const std::vector<Print>& prints, float at, float skip, float most, float fade) {
    std::vector<Print> out;
    for (const Print& p : prints) {
        const float d = p.at - at;
        const float k = fade > 0 ? std::min({1.0f, (d - skip) / fade, (most - d) / fade}) : (d >= skip && d <= most);
        if (!(k > 0)) continue;
        out.push_back(p);
        out.back().alpha = p.alpha * k;
    }
    return out;
}

uint32_t faded(uint32_t argb, float k) {
    const float a = float(argb >> 24) * std::clamp(k, 0.0f, 1.0f);
    return (argb & 0xFFFFFFu) | (uint32_t(std::lround(a)) << 24);
}

std::vector<Point> printShape(const Print& print, float grow) {
    // in world units along its toes (f) and across (r): the sole round f 0.40 (0.80 x 0.44), the heel round f -0.85
    // (0.40 x 0.36) - some 2.5 long, half a cell
    struct Oval {
        float f, along, across;
        int sides;
    };
    const Oval ovals[] = {{0.40f, 0.80f, 0.44f, kSoleSides}, {-0.85f, 0.40f, 0.36f, kHeelSides}};
    const float rx = print.fz, rz = -print.fx;  // across: its right
    std::vector<Point> out;
    for (const Oval& o : ovals)
        for (int k = 0; k < o.sides; ++k) {
            const float a = 6.2831853f * float(k) / float(o.sides);
            const float f = o.f + (o.along + grow) * std::cos(a), r = (o.across + grow) * std::sin(a);
            out.push_back(Point{print.x + print.fx * f + rx * r, 0.0f, print.z + print.fz * f + rz * r});
        }
    return out;
}

size_t footprints(const std::vector<Print>& prints, const Heights& g, int cellSize, float sign, float lift, float grow,
                  const float view[12], const camera::Projection& p, uint32_t argb, float bias,
                  std::vector<Vertex>& out) {
    const size_t before = out.size();
    if (cellSize <= 0) return 0;
    uint32_t colour = argb;  // the print's own
    const auto onGround = [&](float x, float z, Vertex& v) {
        float h = 0;
        if (!g.at(x / float(cellSize) + float(g.w / 2), z / float(cellSize) + float(g.h / 2), h)) return false;
        const float w[3] = {x, sign * h - lift, z};
        return placed(w, view, p, bias, colour, v);
    };
    std::vector<Vertex> one;
    for (const Print& print : prints) {
        colour = faded(argb, print.alpha);
        if (!(colour >> 24)) continue;
        const std::vector<Point> shape = printShape(print, grow);
        one.clear();
        bool whole = true;
        size_t first = 0;
        for (const int sides : {kSoleSides, kHeelSides}) {
            float mx = 0, mz = 0;  // the oval's middle
            for (int k = 0; k < sides; ++k) {
                mx += shape[first + size_t(k)].x / float(sides);
                mz += shape[first + size_t(k)].z / float(sides);
            }
            Vertex middle{};
            whole = whole && onGround(mx, mz, middle);
            for (int k = 0; k < sides && whole; ++k) {
                const Point& a = shape[first + size_t(k)];
                const Point& b = shape[first + size_t((k + 1) % sides)];
                Vertex va{}, vb{};
                whole = onGround(a.x, a.z, va) && onGround(b.x, b.z, vb);
                one.insert(one.end(), {middle, va, vb});
            }
            first += size_t(sides);
        }
        if (whole) out.insert(out.end(), one.begin(), one.end());
    }
    return out.size() - before;
}

size_t ring(float x, float y, float z, float radius, float width, int sides, bool dashed, const float view[12],
            const camera::Projection& p, uint32_t argb, float bias, std::vector<Vertex>& out) {
    const size_t before = out.size();
    const float in = std::max(0.0f, radius - width / 2), outR = radius + width / 2;
    const float step = 6.28318530718f / float(std::max(3, sides));
    for (int i = 0; i < std::max(3, sides); ++i) {
        if (dashed && (i % 2)) continue;
        const float a0 = step * float(i), a1 = step * float(i + 1);
        const float wa[3] = {x + in * std::cos(a0), y, z + in * std::sin(a0)};
        const float wb[3] = {x + outR * std::cos(a0), y, z + outR * std::sin(a0)};
        const float wc[3] = {x + in * std::cos(a1), y, z + in * std::sin(a1)};
        const float wd[3] = {x + outR * std::cos(a1), y, z + outR * std::sin(a1)};
        Vertex a{}, b{}, c{}, d{};
        if (!placed(wa, view, p, bias, argb, a) || !placed(wb, view, p, bias, argb, b) ||
            !placed(wc, view, p, bias, argb, c) || !placed(wd, view, p, bias, argb, d))
            continue;  // behind the near plane
        out.insert(out.end(), {a, b, c, b, d, c});
    }
    return out.size() - before;
}

size_t notches(float x, float y, float z, float radius, float width, const float view[12], const camera::Projection& p,
               uint32_t argb, float bias, std::vector<Vertex>& out) {
    const size_t before = out.size();
    const float from = radius + width / 2, to = from + 0.9f, half = width / 2;
    for (int k = 0; k < 4; ++k) {
        const float ux = k == 0 ? 1.0f : k == 2 ? -1.0f : 0.0f, uz = k == 1 ? 1.0f : k == 3 ? -1.0f : 0.0f;
        const float sx = -uz * half, sz = ux * half;  // across it
        const float wa[3] = {x + ux * from - sx, y, z + uz * from - sz};
        const float wb[3] = {x + ux * from + sx, y, z + uz * from + sz};
        const float wc[3] = {x + ux * to - sx, y, z + uz * to - sz};
        const float wd[3] = {x + ux * to + sx, y, z + uz * to + sz};
        Vertex a{}, b{}, c{}, d{};
        if (!placed(wa, view, p, bias, argb, a) || !placed(wb, view, p, bias, argb, b) ||
            !placed(wc, view, p, bias, argb, c) || !placed(wd, view, p, bias, argb, d))
            continue;
        out.insert(out.end(), {a, b, c, b, d, c});
    }
    return out.size() - before;
}

size_t arrow(float x0, float z0, float x1, float z1, float y, float width, float head, float headWidth,
             const float view[12], const camera::Projection& p, uint32_t argb, float bias, std::vector<Vertex>& out) {
    const size_t before = out.size();
    const float len = std::hypot(x1 - x0, z1 - z0);
    if (len < 1e-3f) return 0;
    const float ux = (x1 - x0) / len, uz = (z1 - z0) / len, sx = -uz, sz = ux;  // along it, across it
    const float neck = std::max(0.0f, len - head);
    if (neck > 0.05f) {  // the shaft
        const float h = width / 2;
        const float wa[3] = {x0 - sx * h, y, z0 - sz * h}, wb[3] = {x0 + sx * h, y, z0 + sz * h};
        const float wc[3] = {x0 + ux * neck - sx * h, y, z0 + uz * neck - sz * h};
        const float wd[3] = {x0 + ux * neck + sx * h, y, z0 + uz * neck + sz * h};
        Vertex a{}, b{}, c{}, d{};
        if (placed(wa, view, p, bias, argb, a) && placed(wb, view, p, bias, argb, b) &&
            placed(wc, view, p, bias, argb, c) && placed(wd, view, p, bias, argb, d))
            out.insert(out.end(), {a, b, c, b, d, c});
    }
    const float h = headWidth / 2;  // the head: its base at the neck, its tip at the end
    const float wa[3] = {x0 + ux * neck - sx * h, y, z0 + uz * neck - sz * h};
    const float wb[3] = {x0 + ux * neck + sx * h, y, z0 + uz * neck + sz * h};
    const float wt[3] = {x1, y, z1};
    Vertex a{}, b{}, t{};
    if (placed(wa, view, p, bias, argb, a) && placed(wb, view, p, bias, argb, b) && placed(wt, view, p, bias, argb, t))
        out.insert(out.end(), {a, b, t});
    return out.size() - before;
}

namespace {

// 3.0.11-beta.103: an arrow's shapes flat on the map (the game's world x, z), put on its heights after
struct Flat {
    float x[3], z[3];
    uint32_t argb;
    uint8_t part;
};

constexpr uint32_t kGold = 0x00E6CB8Au, kWhite = 0x00FFFFFFu, kWall = 0xEBEB7864u;
constexpr float kShadeGrow = 0.06f;  // a cell's

constexpr uint32_t with(uint32_t rgb, uint32_t alpha) { return (alpha << 24) | (rgb & 0xFFFFFFu); }

// Draws one pass of an arrow: the light one, or its shade (each shape `grow` bigger all round - the builders below
// take it - black at half its alpha; the white heads inside their gold edges and the cells' fills left out)
struct Pen {
    std::vector<Flat>& out;
    bool shade;
    float grow, alpha;

    void tri(float ax, float az, float bx, float bz, float cx, float cz, uint32_t argb, uint8_t part) {
        if (shade && (part == kPartHead || part == kPartCellFill)) return;
        const uint32_t c = shade ? faded(argb & 0xFF000000u, 0.5f * alpha) : faded(argb, alpha);
        out.push_back(Flat{{ax, bx, cx}, {az, bz, cz}, c, shade ? kPartShade : part});
    }
    void quad(float ax, float az, float bx, float bz, float cx, float cz, float dx, float dz, uint32_t argb,
              uint8_t part) {
        tri(ax, az, bx, bz, cx, cz, argb, part);
        tri(ax, az, cx, cz, dx, dz, argb, part);
    }
    // a band from a to b, `halfA` / `halfB` to each side at its ends, `extA` / `extB` longer there
    void band(float ax, float az, float bx, float bz, float halfA, float halfB, float extA, float extB, uint32_t argb,
              uint8_t part) {
        const float lx = bx - ax, lz = bz - az, l = std::hypot(lx, lz);
        if (l < 1e-4f) return;
        const float ux = lx / l, uz = lz / l, px = -uz, pz = ux;
        ax -= ux * extA, az -= uz * extA, bx += ux * extB, bz += uz * extB;
        quad(ax - px * halfA, az - pz * halfA, bx - px * halfB, bz - pz * halfB, bx + px * halfB, bz + pz * halfB,
             ax + px * halfA, az + pz * halfA, argb, part);
    }
    // a band cut into pieces of at most `piece` (the ground followed), its ends `ext` longer
    void line(float ax, float az, float bx, float bz, float half, float piece, float ext, uint32_t argb, uint8_t part) {
        const float l = std::hypot(bx - ax, bz - az);
        const int n = std::max(1, int(std::ceil(l / piece)));
        for (int i = 0; i < n; ++i) {
            const float t0 = float(i) / float(n), t1 = float(i + 1) / float(n);
            band(ax + (bx - ax) * t0, az + (bz - az) * t0, ax + (bx - ax) * t1, az + (bz - az) * t1, half, half,
                 i == 0 ? ext : 0.0f, i == n - 1 ? ext : 0.0f, argb, part);
        }
    }
    // a triangle `grow` bigger all round: scaled about its incentre
    void grownTri(float ax, float az, float bx, float bz, float cx, float cz, uint32_t argb, uint8_t part) {
        const float la = std::hypot(cx - bx, cz - bz), lb = std::hypot(ax - cx, az - cz);
        const float lc = std::hypot(bx - ax, bz - az);
        const float per = la + lb + lc, area = std::fabs((bx - ax) * (cz - az) - (cx - ax) * (bz - az)) / 2;
        if (grow > 0 && per > 1e-4f && area > 1e-6f) {
            const float ix = (la * ax + lb * bx + lc * cx) / per, iz = (la * az + lb * bz + lc * cz) / per;
            const float k = (2 * area / per + grow) / (2 * area / per);
            ax = ix + (ax - ix) * k, az = iz + (az - iz) * k, bx = ix + (bx - ix) * k, bz = iz + (bz - iz) * k;
            cx = ix + (cx - ix) * k, cz = iz + (cz - iz) * k;
        }
        tri(ax, az, bx, bz, cx, cz, argb, part);
    }
    void disc(float x, float z, float r, int sides, uint32_t argb, uint8_t part) {
        r += grow;
        for (int i = 0; i < sides; ++i) {
            const float a0 = 6.2831853f * float(i) / float(sides), a1 = 6.2831853f * float(i + 1) / float(sides);
            tri(x, z, x + std::cos(a0) * r, z + std::sin(a0) * r, x + std::cos(a1) * r, z + std::sin(a1) * r, argb,
                part);
        }
    }
    void ring(float x, float z, float r, float width, int sides, uint32_t argb, uint8_t part) {
        const float in = r - width / 2 - grow, out = r + width / 2 + grow;
        for (int i = 0; i < sides; ++i) {
            const float a0 = 6.2831853f * float(i) / float(sides), a1 = 6.2831853f * float(i + 1) / float(sides);
            const float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
            quad(x + c0 * in, z + s0 * in, x + c0 * out, z + s0 * out, x + c1 * out, z + s1 * out, x + c1 * in,
                 z + s1 * in, argb, part);
        }
    }
};

// Where an arrow lies: from the feet along the way (`t` along it, `s` to its left), the cell's middle `len` away
struct Frame {
    float x0, z0, ux, uz, len, cx, cz, cs;
    float x(float t, float s) const { return x0 + ux * t - uz * s; }
    float z(float t, float s) const { return z0 + uz * t + ux * s; }
};

// beta.102's concept arrow: a soft white shaft in half-cell pieces from near the feet, a white head over a gold edge
// with its tip at the cell's near edge, the cell faintly filled and outlined in gold (`mark`)
void conceptLook(Pen& pen, const Frame& f, bool mark) {
    const float cs = f.cs, g = pen.grow;
    const float x1 = f.cx - cs / 2, z1 = f.cz - cs / 2, x2 = f.cx + cs / 2, z2 = f.cz + cs / 2;
    if (mark) {
        pen.tri(x1, z1, x2, z1, x1, z2, kWalkCellFill, kPartCellFill);
        pen.tri(x2, z1, x2, z2, x1, z2, kWalkCellFill, kPartCellFill);
        const float corners[5][2] = {{x1, z1}, {x2, z1}, {x2, z2}, {x1, z2}, {x1, z1}};
        for (int i = 0; i < 4; ++i)
            pen.line(corners[i][0], corners[i][1], corners[i + 1][0], corners[i + 1][1], 0.045f * cs + g, 0.5f * cs,
                     g, kWalkCellLine, kPartCellLine);
    }
    const float headLen = 1.0f * cs, headHalf = 0.55f * cs, tip = f.len - 0.55f * cs, run = tip - 0.45f * cs;
    const float head = run > headLen + 0.1f * cs ? headLen : std::max(0.5f * cs, std::min(headLen, run + 0.25f * cs));
    if (run > headLen + 0.1f * cs) {
        const float shaft = run - headLen;
        const int pieces = std::max(1, int(std::ceil(shaft / (0.5f * cs))));
        for (int i = 0; i < pieces; ++i) {
            const float a = 0.45f * cs + shaft * float(i) / float(pieces);
            const float b = 0.45f * cs + shaft * float(i + 1) / float(pieces);
            pen.band(f.x(a, 0), f.z(a, 0), f.x(b, 0), f.z(b, 0), 0.19f * cs + g, 0.19f * cs + g, i == 0 ? g : 0.0f,
                     0.0f, kWalkShaft, kPartShaft);
        }
    }
    const float base = tip - head, e = 0.12f * cs + g;
    pen.tri(f.x(base - e, headHalf + e), f.z(base - e, headHalf + e), f.x(base - e, -headHalf - e),
            f.z(base - e, -headHalf - e), f.x(tip + e * 1.5f, 0), f.z(tip + e * 1.5f, 0), kWalkEdge, kPartEdge);
    pen.tri(f.x(base, headHalf), f.z(base, headHalf), f.x(base, -headHalf), f.z(base, -headHalf), f.x(tip, 0),
            f.z(tip, 0), kWalkHead, kPartHead);
}

// A: a slim shaft widening from near the feet and coming up out of nothing, a white head edged in gold, the cell's
// gold corners over a faint gold fill (`mark`)
void slimLook(Pen& pen, const Frame& f, bool mark) {
    const float cs = f.cs, g = pen.grow, tip = f.len - 0.55f * cs, half = 0.425f * cs;
    const float head = std::max(0.45f * cs, std::min(0.95f * cs, tip - 0.6f * cs)), base = tip - head;
    const float t0 = 0.6f * cs;
    if (base - t0 > 0.1f * cs) {
        const int n = std::max(1, int(std::ceil((base - t0) / (0.35f * cs))));
        for (int i = 0; i < n; ++i) {
            const float k0 = float(i) / float(n), k1 = float(i + 1) / float(n), a = t0 + (base - t0) * k0;
            const float b = t0 + (base - t0) * k1, km = (k0 + k1) / 2;
            const float h0 = (0.042f + 0.103f * k0) * cs + g, h1 = (0.042f + 0.103f * k1) * cs + g;
            const uint32_t alpha = uint32_t(std::lround(255 * (0.05f + 0.70f * std::pow(km, 1.3f))));
            pen.band(f.x(a, 0), f.z(a, 0), f.x(b, 0), f.z(b, 0), h0, h1, i == 0 ? g : 0.0f, 0.0f, with(kWhite, alpha),
                     kPartShaft);
        }
    }
    const float edge = 0.0625f * cs;
    Pen grown{pen.out, pen.shade, edge + g, pen.alpha};
    grown.grownTri(f.x(base, half), f.z(base, half), f.x(base, -half), f.z(base, -half), f.x(tip, 0), f.z(tip, 0),
                   with(kGold, 0xF5), kPartEdge);
    pen.tri(f.x(base, half), f.z(base, half), f.x(base, -half), f.z(base, -half), f.x(tip, 0), f.z(tip, 0),
            with(kWhite, 0xEB), kPartHead);
    if (!mark) return;
    const float x1 = f.cx - cs / 2, z1 = f.cz - cs / 2, x2 = f.cx + cs / 2, z2 = f.cz + cs / 2, arm = 0.32f * cs;
    pen.tri(x1, z1, x2, z1, x1, z2, with(kGold, 0x1A), kPartCellFill);
    pen.tri(x2, z1, x2, z2, x1, z2, with(kGold, 0x1A), kPartCellFill);
    const float cx[4] = {x1, x2, x2, x1}, cz[4] = {z1, z1, z2, z2};
    for (int i = 0; i < 4; ++i) {
        const float sx = cx[i] == x1 ? 1.0f : -1.0f, sz = cz[i] == z1 ? 1.0f : -1.0f;
        pen.band(cx[i], cz[i], cx[i] + sx * arm, cz[i], 0.035f * cs + g, 0.035f * cs + g, g, g, with(kGold, 0xE6),
                 kPartCellLine);
        pen.band(cx[i], cz[i], cx[i], cz[i] + sz * arm, 0.035f * cs + g, 0.035f * cs + g, g, g, with(kGold, 0xE6),
                 kPartCellLine);
    }
}

// B: up to three chevrons running on toward the cell (faint at the feet, coming in and going out softly), a gold
// diamond in the cell (`mark`); a short way (a wall near) - one chevron, still, before the diamond
void chevronsLook(Pen& pen, const Frame& f, float phase, bool mark) {
    const float cs = f.cs, g = pen.grow, start = 1.25f * cs, span = f.len - 0.85f * cs - start;
    const bool still = span < 0.3f * cs;
    if (!still || f.len >= 1.6f * cs) {
        const int n = still ? 1 : std::clamp(int(std::lround(span / (1.05f * cs))), 1, 3);
        const float half = 0.5f * cs + g, depth = 0.42f * cs, thick = 0.26f * cs + 2 * g;
        for (int i = 0; i < n; ++i) {
            const float u = (float(i) + phase) / float(n);
            const float at = (still ? f.len - 0.6f * cs : start + u * span) + g;
            const float k = still ? 0.95f
                                  : (0.45f + 0.5f * u) * std::min(1.0f, u / 0.2f) * std::min(1.0f, (1 - u) / 0.15f);
            const uint32_t c = with(kWhite, uint32_t(std::lround(255 * std::clamp(k, 0.0f, 1.0f))));
            // the left arm and the right one: two parallelograms sharing the tip's edge
            const float back = at - depth - thick;
            pen.quad(f.x(at, 0), f.z(at, 0), f.x(at - depth, half), f.z(at - depth, half), f.x(back, half),
                     f.z(back, half), f.x(at - thick, 0), f.z(at - thick, 0), c, kPartChevron);
            pen.quad(f.x(at, 0), f.z(at, 0), f.x(at - thick, 0), f.z(at - thick, 0), f.x(back, -half),
                     f.z(back, -half), f.x(at - depth, -half), f.z(at - depth, -half), c, kPartChevron);
        }
    }
    if (!mark) return;
    const float r = 0.3f * cs + g * 1.4142f;  // the diamond along the map's axes, as the cell
    pen.quad(f.cx - r, f.cz, f.cx, f.cz - r, f.cx + r, f.cz, f.cx, f.cz + r, with(kGold, 0xF0), kPartMarker);
}

// C: a ring round the feet, its gold pointer the way; a small gold ring in the cell (`mark`)
void ringLook(Pen& pen, const Frame& f, bool mark) {
    const float cs = f.cs;
    pen.ring(f.x0, f.z0, 1.05f * cs, 0.083f * cs, 40, with(kWhite, 0xBE), kPartRing);
    pen.grownTri(f.x(1.1f * cs, 0.42f * cs), f.z(1.1f * cs, 0.42f * cs), f.x(1.1f * cs, -0.42f * cs),
                 f.z(1.1f * cs, -0.42f * cs), f.x(2.0f * cs, 0), f.z(2.0f * cs, 0), with(kGold, 0xF5), kPartPointer);
    if (mark) pen.ring(f.cx, f.cz, 0.32f * cs, 0.0625f * cs, 24, with(kGold, 0xDC), kPartMarker);
}

// D: dots a half cell apart up to the cell's near edge, bigger and brighter toward it; a gold ring in the cell
// (`mark`)
void dotsLook(Pen& pen, const Frame& f, bool mark) {
    const float cs = f.cs;
    const int n = f.len - 0.5f * cs >= 0.6f * cs ? int((f.len - 1.1f * cs) / (0.5f * cs)) + 1 : 0;
    for (int i = 0; i < n; ++i) {
        const float k = n > 1 ? float(i) / float(n - 1) : 1.0f, at = (0.6f + 0.5f * float(i)) * cs;
        pen.disc(f.x(at, 0), f.z(at, 0), (0.09f + 0.04f * k) * cs, 8,
                 with(kWhite, uint32_t(std::lround(255 * (0.25f + 0.65f * k)))), kPartDot);
    }
    if (mark) pen.ring(f.cx, f.cz, 0.36f * cs, 0.0625f * cs, 24, with(kGold, 0xE6), kPartMarker);
}

// E: the cell alone - softly lit in gold, its gold frame; a small gold wedge at the feet shows the way
void cellLook(Pen& pen, const Frame& f) {
    const float cs = f.cs, g = pen.grow;
    const float x1 = f.cx - cs / 2, z1 = f.cz - cs / 2, x2 = f.cx + cs / 2, z2 = f.cz + cs / 2;
    pen.tri(x1, z1, x2, z1, x1, z2, with(kGold, 0x38), kPartCellFill);
    pen.tri(x2, z1, x2, z2, x1, z2, with(kGold, 0x38), kPartCellFill);
    const float corners[5][2] = {{x1, z1}, {x2, z1}, {x2, z2}, {x1, z2}, {x1, z1}};
    for (int i = 0; i < 4; ++i)
        pen.line(corners[i][0], corners[i][1], corners[i + 1][0], corners[i + 1][1], 0.035f * cs + g, 0.5f * cs, g,
                 with(kGold, 0xF2), kPartCellLine);
    pen.grownTri(f.x(0.75f * cs, 0.32f * cs), f.z(0.75f * cs, 0.32f * cs), f.x(0.75f * cs, -0.32f * cs),
                 f.z(0.75f * cs, -0.32f * cs), f.x(1.5f * cs, 0), f.z(1.5f * cs, 0), with(kGold, 0xF5), kPartPointer);
}

// The flat shapes on the map's heights (`lift` over them: up along -y); none when a corner is off the map
std::vector<Tri> onGround(const std::vector<Flat>& flat, const Heights& g, float cs, float sign, float lift) {
    std::vector<Tri> out;
    out.reserve(flat.size());
    bool ok = true;
    const auto at = [&](float x, float z) {
        float h = 0;
        if (!g.at(x / cs + float(g.w / 2), z / cs + float(g.h / 2), h)) ok = false;
        return Point{x, sign * h - lift, z};
    };
    for (const Flat& t : flat) out.push_back(Tri{at(t.x[0], t.z[0]), at(t.x[1], t.z[1]), at(t.x[2], t.z[2]), t.argb,
                                                 t.part});
    if (!ok) out.clear();
    return out;
}

}  // namespace

std::vector<Tri> walkArrowShape(float x0, float z0, int toX, int toY, const Heights& g, int cellSize, float sign,
                                float lift) {
    if (cellSize <= 0 || g.w <= 0 || g.h <= 0) return {};
    const float cs = float(cellSize);
    float cx = 0, cz = 0;
    cellToWorld(toX, toY, g.w, g.h, cellSize, cx, cz);
    const float dx = cx - x0, dz = cz - z0, len = std::hypot(dx, dz);
    if (len < 0.6f * cs) return {};  // where it stands
    std::vector<Flat> flat;
    Pen pen{flat, false, 0.0f, 1.0f};
    conceptLook(pen, Frame{x0, z0, dx / len, dz / len, len, cx, cz, cs}, true);
    return onGround(flat, g, cs, sign, lift);
}

std::vector<Tri> walkShape(WalkLook look, const WalkArrow& a, const Heights& g, int cellSize, float sign,
                           float lift) {
    const float way = std::hypot(a.wayX, a.wayZ);
    if (look == WalkLook::Off || cellSize <= 0 || g.w <= 0 || g.h <= 0 || a.alpha <= 0 || way < 1e-4f) return {};
    const float cs = float(cellSize), ux = a.wayX / way, uz = a.wayZ / way, len = std::max(0.0f, a.len);
    const Frame f{a.x0, a.z0, ux, uz, len, a.x0 + ux * len, a.z0 + uz * len, cs};
    std::vector<Flat> flat;
    for (const bool shade : {true, false}) {  // the dark shade first, under the rest
        Pen pen{flat, shade, shade ? kShadeGrow * cs : 0.0f, std::min(1.0f, a.alpha)};
        if (len >= 0.6f * cs) {
            switch (look) {
                case WalkLook::Chevrons: chevronsLook(pen, f, a.phase - std::floor(a.phase), a.mark); break;
                case WalkLook::Slim: slimLook(pen, f, a.mark); break;
                case WalkLook::Concept: conceptLook(pen, f, a.mark); break;
                case WalkLook::Ring: ringLook(pen, f, a.mark); break;
                case WalkLook::Dots: dotsLook(pen, f, a.mark); break;
                case WalkLook::Cell: cellLook(pen, f); break;
                case WalkLook::Off: break;
            }
        }
        if (a.blocked) {  // a wall past the cell (or right ahead, standing): a short bar across the way
            const float at = len >= 0.6f * cs ? len + 0.55f * cs : 0.9f * cs;
            pen.line(f.x(at, -0.5f * cs), f.z(at, -0.5f * cs), f.x(at, 0.5f * cs), f.z(at, 0.5f * cs),
                     0.05f * cs + pen.grow, 0.5f * cs, pen.grow, kWall, kPartWall);
        }
    }
    return onGround(flat, g, cs, sign, lift);
}

void glide(WalkGlide& g, const WalkAim& aim, float dtMs) {
    constexpr float kTurnMs = 80, kLengthMs = 100;
    const float way = std::hypot(aim.wayX, aim.wayZ);
    if (way < 1e-6f) return;
    const float ux = aim.wayX / way, uz = aim.wayZ / way, angle = std::atan2(uz, ux);
    const float len = aim.cut ? std::max(0.0f, (aim.cellX - aim.x0) * ux + (aim.cellZ - aim.z0) * uz) : aim.lead;
    if (g.has && std::hypot(aim.x0 - g.x0, aim.z0 - g.z0) > std::max(aim.lead, 1.0f)) g.has = false;  // a warp
    if (!g.has) {
        g = WalkGlide{true, angle, len, aim.x0, aim.z0};
        return;
    }
    if (aim.cut) g.len -= (aim.x0 - g.x0) * ux + (aim.z0 - g.z0) * uz;  // the wall's end stays on the ground
    g.x0 = aim.x0;
    g.z0 = aim.z0;
    const float dt = std::max(0.0f, dtMs);
    g.angle += std::remainder(angle - g.angle, 6.2831853f) * (1.0f - std::exp(-dt / kTurnMs));
    g.angle = std::remainder(g.angle, 6.2831853f);
    g.len += (len - g.len) * (1.0f - std::exp(-dt / kLengthMs));
}

size_t placeTris(const std::vector<Tri>& tris, const float view[12], const camera::Projection& p, float bias,
                 std::vector<Vertex>& out) {
    const size_t before = out.size();
    for (const Tri& t : tris) {
        const float a[3] = {t.a.x, t.a.y, t.a.z}, b[3] = {t.b.x, t.b.y, t.b.z}, c[3] = {t.c.x, t.c.y, t.c.z};
        Vertex va{}, vb{}, vc{};
        if (placed(a, view, p, bias, t.argb, va) && placed(b, view, p, bias, t.argb, vb) &&
            placed(c, view, p, bias, t.argb, vc))
            out.insert(out.end(), {va, vb, vc});
    }
    return out.size() - before;
}

size_t chevron(float cx, float tipY, float w, float h, uint32_t argb, std::vector<Vertex>& out) {
    out.push_back(Vertex{cx - w / 2, tipY - h, 0.0f, 1.0f, argb, 0u, 0.0f, 0.0f});
    out.push_back(Vertex{cx + w / 2, tipY - h, 0.0f, 1.0f, argb, 0u, 0.0f, 0.0f});
    out.push_back(Vertex{cx, tipY, 0.0f, 1.0f, argb, 0u, 0.0f, 0.0f});
    return 3;
}

}  // namespace uagfx::groundway
