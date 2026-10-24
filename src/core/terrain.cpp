#include "core/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace uagfx::terrain {
namespace {

// The ground's corners as one grid: (width + 1) x (height + 1) heights, NAN where the cells around a corner part
// (a step)
struct Grid {
    int w = 0, h = 0;
    std::vector<float> at;
    float get(int i, int j) const {
        if (i < 0 || j < 0 || i > w || j > h) return NAN;
        return at[size_t(j) * size_t(w + 1) + size_t(i)];
    }
};

Grid joinedCorners(const gnd::Ground& g) {
    Grid grid;
    grid.w = g.width;
    grid.h = g.height;
    grid.at.assign(size_t(g.width + 1) * size_t(g.height + 1), NAN);
    for (int j = 0; j <= g.height; ++j)
        for (int i = 0; i <= g.width; ++i) {
            float lo = 1e30f, hi = -1e30f, sum = 0.0f;
            int n = 0;
            // the cells around the corner and which of their corners it is
            const int cx[4] = {i - 1, i, i - 1, i}, cy[4] = {j - 1, j - 1, j, j}, corner[4] = {3, 2, 1, 0};
            for (int k = 0; k < 4; ++k) {
                if (cx[k] < 0 || cy[k] < 0 || cx[k] >= g.width || cy[k] >= g.height) continue;
                const gnd::Cell& c = g.cells[size_t(cy[k]) * size_t(g.width) + size_t(cx[k])];
                if (c.top < 0) continue;
                const float v = c.h[corner[k]];
                lo = std::min(lo, v), hi = std::max(hi, v), sum += v, ++n;
            }
            if (n > 0 && hi - lo <= kJoin) grid.at[size_t(j) * size_t(g.width + 1) + size_t(i)] = sum / float(n);
        }
    return grid;
}

float catmull(float p0, float p1, float p2, float p3, float t) {
    const float t2 = t * t, t3 = t2 * t;
    return 0.5f * (2.0f * p1 + (p2 - p0) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (3.0f * p1 - p0 - 3.0f * p2 + p3) * t3);
}

// A cell's height at (fu, fv) in it: the patch over the 4 x 4 corners around it when they are all whole ground, else
// the game's own plane between its four corners
float heightAt(const gnd::Cell& c, const Grid& grid, int x, int y, float fu, float fv, bool smooth) {
    if (smooth) {
        float rows[4];
        for (int r = 0; r < 4; ++r)
            rows[r] = catmull(grid.get(x - 1, y - 1 + r), grid.get(x, y - 1 + r), grid.get(x + 1, y - 1 + r),
                              grid.get(x + 2, y - 1 + r), fu);
        return catmull(rows[0], rows[1], rows[2], rows[3], fv);
    }
    const float top = c.h[0] + (c.h[1] - c.h[0]) * fu, bottom = c.h[2] + (c.h[3] - c.h[2]) * fu;
    return top + (bottom - top) * fv;
}

bool patchWhole(const Grid& grid, int x, int y) {
    for (int j = y - 1; j <= y + 2; ++j)
        for (int i = x - 1; i <= x + 2; ++i)
            if (std::isnan(grid.get(i, j))) return false;
    return true;
}

void normalize(float* n) {
    const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (l > 1e-12f) {
        n[0] /= l, n[1] /= l, n[2] /= l;
    } else {
        n[0] = 0, n[1] = -1, n[2] = 0;
    }
}

struct Builder {
    const gnd::Ground& g;
    const Atlas& atlas;
    std::map<int, std::vector<uint32_t>> byTexture;  // indices, a texture's
    std::vector<Vertex> vertices;

    void lightmapUv(uint16_t lightmap, float fu, float fv, float* out) const {
        const int cols = std::max(atlas.width / 8, 1);
        const float x = float(lightmap % cols) * 8.0f + 1.0f + fu * 6.0f;
        const float y = float(lightmap / cols) * 8.0f + 1.0f + fv * 6.0f;
        out[0] = atlas.width > 0 ? x / float(atlas.width) : 0.0f;
        out[1] = atlas.height > 0 ? y / float(atlas.height) : 0.0f;
    }

    void quad(int texture, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {  // a b / c d
        std::vector<uint32_t>& ix = byTexture[texture];
        ix.insert(ix.end(), {a, b, c, b, d, c});
    }
};

}  // namespace

Atlas lightmapAtlas(const gnd::Ground& g) {
    Atlas a;
    const int cols = kAtlasWidth / 8;
    const int rows = std::max(1, int((g.lightmaps.size() + size_t(cols) - 1) / size_t(cols)));
    a.width = kAtlasWidth;
    a.height = rows * 8;
    a.argb.assign(size_t(a.width) * size_t(a.height), 0xFF000000u);
    for (size_t i = 0; i < g.lightmaps.size(); ++i) {
        const gnd::Lightmap& l = g.lightmaps[i];
        const int ox = int(i % size_t(cols)) * 8, oy = int(i / size_t(cols)) * 8;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const int k = y * 8 + x;
                a.argb[size_t(oy + y) * size_t(a.width) + size_t(ox + x)] =
                    uint32_t(l.brightness[k]) << 24 | uint32_t(l.rgb[k * 3]) << 16 | uint32_t(l.rgb[k * 3 + 1]) << 8 |
                    l.rgb[k * 3 + 2];
            }
    }
    return a;
}

Mesh build(const gnd::Ground& g, int subdivide, const Atlas& atlas) {
    const int n = std::clamp(subdivide, 1, 8);
    const Grid grid = joinedCorners(g);
    Builder b{g, atlas, {}, {}};
    const float half[2] = {float(g.width) / 2.0f, float(g.height) / 2.0f};
    const float step = 1.0f / float(n), e = 0.05f;
    for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x) {
            const gnd::Cell& c = g.cells[size_t(y) * size_t(g.width) + size_t(x)];
            if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) continue;
            const gnd::Surface& s = g.surfaces[size_t(c.top)];
            const bool smooth = n > 1 && patchWhole(grid, x, y);
            const uint32_t first = uint32_t(b.vertices.size());
            for (int j = 0; j <= n; ++j)
                for (int i = 0; i <= n; ++i) {
                    const float fu = float(i) * step, fv = float(j) * step;
                    Vertex v{};
                    v.pos[0] = (float(x) + fu - half[0]) * g.zoom;
                    v.pos[1] = heightAt(c, grid, x, y, fu, fv, smooth);
                    v.pos[2] = (float(y) + fv - half[1]) * g.zoom;
                    // its slope: the height's change along x and z (world units)
                    const float dx = (heightAt(c, grid, x, y, std::min(fu + e, 1.0f), fv, smooth) -
                                      heightAt(c, grid, x, y, std::max(fu - e, 0.0f), fv, smooth)) /
                                     ((std::min(fu + e, 1.0f) - std::max(fu - e, 0.0f)) * g.zoom);
                    const float dz = (heightAt(c, grid, x, y, fu, std::min(fv + e, 1.0f), smooth) -
                                      heightAt(c, grid, x, y, fu, std::max(fv - e, 0.0f), smooth)) /
                                     ((std::min(fv + e, 1.0f) - std::max(fv - e, 0.0f)) * g.zoom);
                    v.normal[0] = dx, v.normal[1] = -1.0f, v.normal[2] = dz;
                    normalize(v.normal);
                    const float top = s.u[0] + (s.u[1] - s.u[0]) * fu, bottom = s.u[2] + (s.u[3] - s.u[2]) * fu;
                    const float vt = s.v[0] + (s.v[1] - s.v[0]) * fu, vb = s.v[2] + (s.v[3] - s.v[2]) * fu;
                    v.uv[0] = top + (bottom - top) * fv;
                    v.uv[1] = vt + (vb - vt) * fv;
                    b.lightmapUv(s.lightmap, fu, fv, v.lm);
                    v.color = s.color;
                    b.vertices.push_back(v);
                }
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i) {
                    const uint32_t a = first + uint32_t(j * (n + 1) + i);
                    b.quad(s.texture, a, a + 1, a + uint32_t(n + 1), a + uint32_t(n + 2));
                }
        }
    // The walls as the game has them: between a cell's side and its neighbour's, plain quads
    const auto wall = [&](int surface, const float* p0, const float* p1, const float* p2, const float* p3) {
        if (surface < 0 || size_t(surface) >= g.surfaces.size()) return;
        const gnd::Surface& s = g.surfaces[size_t(surface)];
        const float* p[4] = {p0, p1, p2, p3};
        float nrm[3];  // across the wall, towards its lower side
        const float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2];
        const float bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
        nrm[0] = ay * bz - az * by, nrm[1] = az * bx - ax * bz, nrm[2] = ax * by - ay * bx;
        normalize(nrm);
        const uint32_t first = uint32_t(b.vertices.size());
        for (int k = 0; k < 4; ++k) {
            Vertex v{};
            std::copy(p[k], p[k] + 3, v.pos);
            std::copy(nrm, nrm + 3, v.normal);
            v.uv[0] = s.u[k];
            v.uv[1] = s.v[k];
            b.lightmapUv(s.lightmap, float(k & 1), float(k >> 1), v.lm);
            v.color = s.color;
            b.vertices.push_back(v);
        }
        b.quad(s.texture, first, first + 1, first + 2, first + 3);
    };
    for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x) {
            const gnd::Cell& c = g.cells[size_t(y) * size_t(g.width) + size_t(x)];
            const float x0 = (float(x) - half[0]) * g.zoom, x1 = x0 + g.zoom;
            const float z0 = (float(y) - half[1]) * g.zoom, z1 = z0 + g.zoom;
            if (c.right >= 0 && x + 1 < g.width) {  // its right side down (or up) to the next cell's left
                const gnd::Cell& r = g.cells[size_t(y) * size_t(g.width) + size_t(x + 1)];
                const float a[3] = {x1, c.h[1], z0}, b2[3] = {x1, c.h[3], z1}, d[3] = {x1, r.h[0], z0},
                            e2[3] = {x1, r.h[2], z1};
                wall(c.right, a, b2, d, e2);
            }
            if (c.front >= 0 && y + 1 < g.height) {  // its far side to the next row's cell
                const gnd::Cell& f = g.cells[size_t(y + 1) * size_t(g.width) + size_t(x)];
                const float a[3] = {x0, c.h[2], z1}, b2[3] = {x1, c.h[3], z1}, d[3] = {x0, f.h[0], z1},
                            e2[3] = {x1, f.h[1], z1};
                wall(c.front, a, b2, d, e2);
            }
        }
    Mesh m;
    m.vertices = std::move(b.vertices);
    for (auto& t : b.byTexture) {
        Batch batch;
        batch.texture = t.first;
        batch.firstIndex = uint32_t(m.indices.size());
        batch.indexCount = uint32_t(t.second.size());
        m.indices.insert(m.indices.end(), t.second.begin(), t.second.end());
        m.batches.push_back(batch);
    }
    return m;
}

}  // namespace uagfx::terrain
