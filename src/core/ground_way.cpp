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

std::vector<Tri> walkArrowShape(float x0, float z0, int toX, int toY, const Heights& g, int cellSize, float sign,
                                float lift) {
    std::vector<Tri> out;
    if (cellSize <= 0 || g.w <= 0 || g.h <= 0) return out;
    const float cs = float(cellSize);
    const float cx = (float(toX) + 0.5f - float(g.w / 2)) * cs, cz = (float(toY) + 0.5f - float(g.h / 2)) * cs;
    const float dx = cx - x0, dz = cz - z0, len = std::hypot(dx, dz);
    if (len < 0.6f * cs) return out;  // where it stands
    const float ux = dx / len, uz = dz / len, sx = -uz, sz = ux;  // along the way, across it
    bool ok = true;
    const auto at = [&](float x, float z) {  // the ground there (`lift` over it: up along -y)
        float h = 0;
        if (!g.at(x / cs + float(g.w / 2), z / cs + float(g.h / 2), h)) ok = false;
        return Point{x, sign * h - lift, z};
    };
    const auto tri = [&](const Point& a, const Point& b, const Point& c, uint32_t argb) {
        out.push_back(Tri{a, b, c, argb});
    };
    // a band from a to b `half` to each side, its corners on the ground
    const auto band = [&](float ax, float az, float bx, float bz, float half, uint32_t argb) {
        const float lx = bx - ax, lz = bz - az, l = std::hypot(lx, lz);
        if (l < 1e-4f) return;
        const float px = -lz / l * half, pz = lx / l * half;
        const Point a1 = at(ax - px, az - pz), a2 = at(ax + px, az + pz), b1 = at(bx - px, bz - pz),
                    b2 = at(bx + px, bz + pz);
        tri(a1, a2, b1, argb);
        tri(a2, b2, b1, argb);
    };
    // the cell: faintly filled, its square outlined in gold (each side in two pieces over the heights)
    const float x1 = cx - cs / 2, z1 = cz - cs / 2, x2 = cx + cs / 2, z2 = cz + cs / 2;
    tri(at(x1, z1), at(x2, z1), at(x1, z2), kWalkCellFill);
    tri(at(x2, z1), at(x2, z2), at(x1, z2), kWalkCellFill);
    const float line = 0.045f * cs;
    const float corners[5][2] = {{x1, z1}, {x2, z1}, {x2, z2}, {x1, z2}, {x1, z1}};
    for (int i = 0; i < 4; ++i) {
        const float mx = (corners[i][0] + corners[i + 1][0]) / 2, mz = (corners[i][1] + corners[i + 1][1]) / 2;
        band(corners[i][0], corners[i][1], mx, mz, line, kWalkCellLine);
        band(mx, mz, corners[i + 1][0], corners[i + 1][1], line, kWalkCellLine);
    }
    // the shaft from near the feet, then the head with its tip at the cell's near edge
    const float headLen = 1.0f * cs, headHalf = 0.55f * cs;
    const float startX = x0 + ux * 0.45f * cs, startZ = z0 + uz * 0.45f * cs;
    const float tipX = cx - ux * 0.55f * cs, tipZ = cz - uz * 0.55f * cs;
    const float run = (tipX - startX) * ux + (tipZ - startZ) * uz;
    const float head = run > headLen + 0.1f * cs ? headLen : std::max(0.5f * cs, std::min(headLen, run + 0.25f * cs));
    if (run > headLen + 0.1f * cs) {
        const float shaft = run - headLen;
        const int pieces = std::max(1, int(std::ceil(shaft / (0.5f * cs))));
        for (int i = 0; i < pieces; ++i) {
            const float a = shaft * float(i) / float(pieces), b = shaft * float(i + 1) / float(pieces);
            band(startX + ux * a, startZ + uz * a, startX + ux * b, startZ + uz * b, 0.19f * cs, kWalkShaft);
        }
    }
    const float baseX = tipX - ux * head, baseZ = tipZ - uz * head, e = 0.12f * cs;
    tri(at(baseX - ux * e + sx * (headHalf + e), baseZ - uz * e + sz * (headHalf + e)),
        at(baseX - ux * e - sx * (headHalf + e), baseZ - uz * e - sz * (headHalf + e)),
        at(tipX + ux * e * 1.5f, tipZ + uz * e * 1.5f), kWalkEdge);
    tri(at(baseX + sx * headHalf, baseZ + sz * headHalf), at(baseX - sx * headHalf, baseZ - sz * headHalf),
        at(tipX, tipZ), kWalkHead);
    if (!ok) out.clear();  // off the map somewhere
    return out;
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
