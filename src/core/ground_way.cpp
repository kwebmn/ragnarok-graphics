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

}  // namespace uagfx::groundway
