#include "core/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

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

// 3.0.4-beta.29: the plain ground's height in a cell - its two triangles as the plain mesh has them (quad(): a b c,
// b d c - from corner 1 to corner 2)
float plainAt(const gnd::Cell& c, float fu, float fv) {
    if (fu + fv <= 1.0f) return c.h[0] + (c.h[1] - c.h[0]) * fu + (c.h[2] - c.h[0]) * fv;
    return c.h[3] + (c.h[2] - c.h[3]) * (1.0f - fu) + (c.h[1] - c.h[3]) * (1.0f - fv);
}

// 3.0.6-beta.3: a point of cell (x, y) at (fu, fv) kept no higher than the plain cells - its cell, or one it shares
// (on an edge or a corner), has a model resting on it; then its plain height from the corners as one grid (the ones
// the cells share), so that a shared point comes out the same from either cell
bool underModel(const std::vector<uint8_t>* resting, const gnd::Ground& g, int x, int y, float fu, float fv) {
    if (!resting) return false;
    const int x0 = fu < 1e-4f ? x - 1 : x, x1 = fu > 1.0f - 1e-4f ? x + 1 : x;
    const int y0 = fv < 1e-4f ? y - 1 : y, y1 = fv > 1.0f - 1e-4f ? y + 1 : y;
    for (int cy = y0; cy <= y1; ++cy)
        for (int cx = x0; cx <= x1; ++cx)
            if (cx >= 0 && cy >= 0 && cx < g.width && cy < g.height &&
                (*resting)[size_t(cy) * size_t(g.width) + size_t(cx)])
                return true;
    return false;
}

float jointPlainAt(const Grid& grid, int x, int y, float fu, float fv) {  // plainAt over the joined corners
    const float h0 = grid.get(x, y), h1 = grid.get(x + 1, y), h2 = grid.get(x, y + 1), h3 = grid.get(x + 1, y + 1);
    if (fu + fv <= 1.0f) return h0 + (h1 - h0) * fu + (h2 - h0) * fv;
    return h3 + (h2 - h3) * (1.0f - fu) + (h1 - h3) * (1.0f - fv);
}

// 3.0.6-beta.3: heightAt kept no higher (up along -y) than the joined corners' plain cells where `keep`
float surfaceAt(const gnd::Cell& c, const Grid& grid, int x, int y, float fu, float fv, bool smooth, bool keep) {
    const float h = heightAt(c, grid, x, y, fu, fv, smooth);
    return keep && smooth ? std::max(h, jointPlainAt(grid, x, y, fu, fv)) : h;
}

bool patchWhole(const Grid& grid, int x, int y) {
    for (int j = y - 1; j <= y + 2; ++j)
        for (int i = x - 1; i <= x + 2; ++i)
            if (std::isnan(grid.get(i, j))) return false;
    return true;
}

// 3.0.4-beta.12, the strips (as core/seams.cpp's): a tile ending at 1 and the next starting at 0 is the same place
bool same(float a, float b) {
    const float d = a - b;
    return std::fabs(d - std::round(d)) < 0.02f;
}
// a place of the neighbour's square past the shared edge folded back into it - its texture mirrored across the edge
float fold(float x) { return x < 0.0f ? -x : x > 1.0f ? 2.0f - x : x; }
// a place of a square kept off its outermost kBand: the textures' outer texels are their dark rims and what the next
// quarter of an atlas bleeds in
constexpr float kBand = 0.03f;
float inset(float x) { return x < kBand ? 2.0f * kBand - x : x > 1.0f - kBand ? 2.0f * (1.0f - kBand) - x : x; }
void uvAt(const gnd::Surface& s, float fu, float fv, float* out) {
    const float top = s.u[0] + (s.u[1] - s.u[0]) * fu, bottom = s.u[2] + (s.u[3] - s.u[2]) * fu;
    const float vt = s.v[0] + (s.v[1] - s.v[0]) * fu, vb = s.v[2] + (s.v[3] - s.v[2]) * fu;
    out[0] = top + (bottom - top) * fv;
    out[1] = vt + (vb - vt) * fv;
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
    struct Part {  // a texture's vertices and its quads' indices into them
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
    std::map<int, Part> parts, bands, strips;  // by texture: the ground's; 3.0.4-beta.12 the own bands', the strips'

    void lightmapUv(uint16_t lightmap, float fu, float fv, float* out) const {
        const int cols = std::max(atlas.width / 8, 1);
        const float x = float(lightmap % cols) * 8.0f + 1.0f + fu * 6.0f;
        const float y = float(lightmap / cols) * 8.0f + 1.0f + fv * 6.0f;
        out[0] = atlas.width > 0 ? x / float(atlas.width) : 0.0f;
        out[1] = atlas.height > 0 ? y / float(atlas.height) : 0.0f;
    }

    static void quad(Part& p, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {  // a b / c d
        p.indices.insert(p.indices.end(), {a, b, c, b, d, c});
    }
};

// 3.0.4-beta.12: the strips inside cell (x, y) along its edges with the neighbours that look different (terrain.hpp)
void blendsOf(Builder& b, const Grid& grid, int x, int y, int n, const std::vector<uint8_t>* laid,
              const std::vector<uint8_t>* resting) {
    const gnd::Ground& g = b.g;
    const auto cellAt = [&](int cx, int cy) -> const gnd::Cell* {
        if (cx < 0 || cy < 0 || cx >= g.width || cy >= g.height) return nullptr;
        const gnd::Cell& c = g.cells[size_t(cy) * size_t(g.width) + size_t(cx)];
        return c.top >= 0 && size_t(c.top) < g.surfaces.size() ? &c : nullptr;
    };
    const auto handLaid = [&](int cx, int cy) { return laid && (*laid)[size_t(cy) * size_t(g.width) + size_t(cx)]; };
    const gnd::Cell* c = cellAt(x, y);
    if (!c || handLaid(x, y)) return;
    const gnd::Surface& s = g.surfaces[size_t(c->top)];
    const bool smooth = patchWhole(grid, x, y);
    const float half[2] = {float(g.width) / 2.0f, float(g.height) / 2.0f};
    const float step = 1.0f / float(n);
    const int rows = (n + 1) / 2;  // the cell's own rows that reach its middle
    // the four ways: this cell's corners on the shared edge and the neighbour's same corners
    struct Way {
        int dx, dy, mine[2], theirs[2];
    };
    static const Way kWays[4] = {{1, 0, {1, 3}, {0, 2}}, {-1, 0, {0, 2}, {1, 3}},
                                 {0, 1, {2, 3}, {0, 1}}, {0, -1, {0, 1}, {2, 3}}};
    for (const Way& w : kWays) {
        const int nx = x + w.dx, ny = y + w.dy;
        const gnd::Cell* o = cellAt(nx, ny);
        if (!o || handLaid(nx, ny)) continue;
        bool wall = false;  // a step between the two
        for (int k = 0; k < 2; ++k) wall = wall || std::fabs(c->h[w.mine[k]] - o->h[w.theirs[k]]) > 0.05f * g.zoom;
        if (wall) continue;
        const gnd::Surface& t = g.surfaces[size_t(o->top)];
        const bool one = s.texture == t.texture;
        if (one) {  // one texture: a seam only where its coordinates jump at the edge
            bool alike = true;
            for (int k = 0; k < 2; ++k)
                alike = alike && same(s.u[w.mine[k]], t.u[w.theirs[k]]) && same(s.v[w.mine[k]], t.v[w.theirs[k]]);
            if (alike) continue;
        }
        const bool low = x < nx || (x == nx && y < ny);  // the side whose noise counts up for the other
        // how much a place is the neighbour's: its cells (the neighbour alone where both have one texture) 1 at their
        // middles, bilinear between the cells' middles - 0.5 at the edge, the line rounds the cells' corners off
        const auto theirs = [&](int cx, int cy) {
            if (cx == nx && cy == ny) return 1.0f;
            const gnd::Cell* k = one ? nullptr : cellAt(cx, cy);
            return k && g.surfaces[size_t(k->top)].texture == t.texture ? 1.0f : 0.0f;
        };
        const auto claim = [&](float fu, float fv) {
            const float gx = float(x) + fu - 0.5f, gy = float(y) + fv - 0.5f;
            const int x0 = int(std::floor(gx)), y0 = int(std::floor(gy));
            const float fx = gx - float(x0), fy = gy - float(y0);
            const float top = theirs(x0, y0) + (theirs(x0 + 1, y0) - theirs(x0, y0)) * fx;
            const float bottom = theirs(x0, y0 + 1) + (theirs(x0 + 1, y0 + 1) - theirs(x0, y0 + 1)) * fx;
            return top + (bottom - top) * fy;
        };
        // (u, v) of the cell's grid point `across` rows from the edge, `along` it
        const auto gridOf = [&](int along, int across, int& gi, int& gj) {
            gi = w.dx == 1 ? n - across : w.dx == -1 ? across : along;
            gj = w.dy == 1 ? n - across : w.dy == -1 ? across : along;
        };
        const auto placed = [&](float fu, float fv, Vertex& v) {
            v.pos[0] = (float(x) + fu - half[0]) * g.zoom;
            v.pos[1] = surfaceAt(*c, grid, x, y, fu, fv, smooth, underModel(resting, g, x, y, fu, fv));  // 3.0.6-beta.3
            v.pos[2] = (float(y) + fv - half[1]) * g.zoom;
            v.plain = plainAt(*c, fu, fv);
            v.normal[0] = 0.0f, v.normal[1] = -1.0f, v.normal[2] = 0.0f;
            b.lightmapUv(s.lightmap, fu, fv, v.lm);
        };
        // its own band: its texture kept off its rim, over 2 kBand from the edge - whole there, then fading (shade()).
        // Three rows: the edge, kBand where the texture is mirrored back (inset), 2 kBand - with two (beta.12) both
        // took one place of the texture, one column of its texels smeared over the band at every seam
        Builder::Part& band = b.bands[s.texture];
        uint32_t first = uint32_t(band.vertices.size());
        for (int j = 0; j <= 2; ++j)
            for (int i = 0; i <= n; ++i) {
                int gi = 0, gj = 0;
                gridOf(i, 0, gi, gj);
                float fu = float(gi) * step, fv = float(gj) * step;  // on the edge: as the ground's own points
                if (w.dx) fu += float(j) * (w.dx > 0 ? -kBand : kBand);
                if (w.dy) fv += float(j) * (w.dy > 0 ? -kBand : kBand);
                Vertex v{};
                placed(fu, fv, v);
                uvAt(s, inset(fu), inset(fv), v.uv);
                v.color = s.color | 0xFF000000u;
                v.blend = 0xFF0000u | (low ? 0xFF00u : 0u) | uint32_t(j * 255 / 2);
                band.vertices.push_back(v);
            }
        for (int j = 0; j < 2; ++j)
            for (int i = 0; i < n; ++i) {
                const uint32_t a = first + uint32_t(j * (n + 1) + i);
                Builder::quad(band, a, a + 1, a + uint32_t(n + 1), a + uint32_t(n + 2));
            }
        // the neighbour's strip on the cell's own grid points (the ground's own, the same arithmetic), its quads split
        // as the ground's: from (gi + 1, gj) to (gi, gj + 1)
        Builder::Part& strip = b.strips[t.texture];
        first = uint32_t(strip.vertices.size());
        for (int j = 0; j <= rows; ++j)
            for (int i = 0; i <= n; ++i) {
                int gi = 0, gj = 0;
                gridOf(i, j, gi, gj);
                const float fu = float(gi) * step, fv = float(gj) * step;
                Vertex v{};
                placed(fu, fv, v);
                uvAt(t, inset(fold(fu - float(w.dx))), inset(fold(fv - float(w.dy))), v.uv);
                const float f = float(j) * step > 0.5f + 1e-4f ? 0.0f : std::clamp(claim(fu, fv), 0.0f, 1.0f);
                v.color = (s.color & 0xFFFFFFu) | uint32_t(255.0f * f + 0.5f) << 24;
                v.blend = (low ? 0xFF00u : 0u) | uint32_t(255.0f * float(j) / float(rows) + 0.5f);
                strip.vertices.push_back(v);
            }
        const auto at = [&](int gi, int gj) {  // the strip's point at the cell's grid point
            const int along = w.dx ? gj : gi;
            const int across = w.dx == 1 ? n - gi : w.dx == -1 ? gi : w.dy == 1 ? n - gj : gj;
            return first + uint32_t(across * (n + 1) + along);
        };
        for (int j = 0; j < rows; ++j)
            for (int i = 0; i < n; ++i) {
                int gi0 = 0, gj0 = 0, gi1 = 0, gj1 = 0;
                gridOf(i, j, gi0, gj0);
                gridOf(i + 1, j + 1, gi1, gj1);
                const int gi = std::min(gi0, gi1), gj = std::min(gj0, gj1);
                Builder::quad(strip, at(gi, gj), at(gi + 1, gj), at(gi, gj + 1), at(gi + 1, gj + 1));
            }
    }
}

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

Mesh build(const gnd::Ground& g, int subdivide, const Atlas& atlas, const std::vector<uint8_t>* laid,
           const std::vector<uint8_t>* resting) {
    const int n = std::clamp(subdivide, 1, 8);
    if (laid && laid->size() != g.cells.size()) laid = nullptr;
    if (resting && resting->size() != g.cells.size()) resting = nullptr;
    const Grid grid = joinedCorners(g);
    const float half[2] = {float(g.width) / 2.0f, float(g.height) / 2.0f};
    const float step = 1.0f / float(n), e = 0.05f;
    Mesh m;
    for (int cy0 = 0; cy0 < g.height; cy0 += kChunk)
        for (int cx0 = 0; cx0 < g.width; cx0 += kChunk) {
            Builder b{g, atlas, {}, {}, {}};
            const int cx1 = std::min(cx0 + kChunk, g.width), cy1 = std::min(cy0 + kChunk, g.height);
            for (int y = cy0; y < cy1; ++y)
                for (int x = cx0; x < cx1; ++x) {
                    const gnd::Cell& c = g.cells[size_t(y) * size_t(g.width) + size_t(x)];
                    if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) continue;
                    const gnd::Surface& s = g.surfaces[size_t(c.top)];
                    const bool smooth = n > 1 && patchWhole(grid, x, y);
                    Builder::Part& part = b.parts[s.texture];
                    const uint32_t first = uint32_t(part.vertices.size());
                    for (int j = 0; j <= n; ++j)
                        for (int i = 0; i <= n; ++i) {
                            const float fu = float(i) * step, fv = float(j) * step;
                            Vertex v{};
                            v.pos[0] = (float(x) + fu - half[0]) * g.zoom;
                            // 3.0.6-beta.3: under a model no higher than the plain cells (its slope taken the same)
                            const bool keep = underModel(resting, g, x, y, fu, fv);
                            v.pos[1] = surfaceAt(c, grid, x, y, fu, fv, smooth, keep);
                            v.pos[2] = (float(y) + fv - half[1]) * g.zoom;
                            v.plain = n > 1 ? plainAt(c, fu, fv) : v.pos[1];
                            // its slope: the height's change along x and z (world units)
                            const float u0 = std::max(fu - e, 0.0f), u1 = std::min(fu + e, 1.0f);
                            const float w0 = std::max(fv - e, 0.0f), w1 = std::min(fv + e, 1.0f);
                            const float dx = (surfaceAt(c, grid, x, y, u1, fv, smooth, keep) -
                                              surfaceAt(c, grid, x, y, u0, fv, smooth, keep)) / ((u1 - u0) * g.zoom);
                            const float dz = (surfaceAt(c, grid, x, y, fu, w1, smooth, keep) -
                                              surfaceAt(c, grid, x, y, fu, w0, smooth, keep)) / ((w1 - w0) * g.zoom);
                            v.normal[0] = dx, v.normal[1] = -1.0f, v.normal[2] = dz;
                            normalize(v.normal);
                            const float top = s.u[0] + (s.u[1] - s.u[0]) * fu, bottom = s.u[2] + (s.u[3] - s.u[2]) * fu;
                            const float vt = s.v[0] + (s.v[1] - s.v[0]) * fu, vb = s.v[2] + (s.v[3] - s.v[2]) * fu;
                            v.uv[0] = top + (bottom - top) * fv;
                            v.uv[1] = vt + (vb - vt) * fv;
                            b.lightmapUv(s.lightmap, fu, fv, v.lm);
                            v.color = s.color;
                            part.vertices.push_back(v);
                        }
                    for (int j = 0; j < n; ++j)
                        for (int i = 0; i < n; ++i) {
                            const uint32_t a = first + uint32_t(j * (n + 1) + i);
                            Builder::quad(part, a, a + 1, a + uint32_t(n + 1), a + uint32_t(n + 2));
                        }
                }
            // The walls as the game has them: between a cell's side and its neighbour's, plain quads
            const auto wall = [&](int surface, const float* p0, const float* p1, const float* p2, const float* p3) {
                if (surface < 0 || size_t(surface) >= g.surfaces.size()) return;
                const gnd::Surface& s = g.surfaces[size_t(surface)];
                const float* p[4] = {p0, p1, p2, p3};
                float nrm[3];
                const float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2];
                const float bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
                nrm[0] = ay * bz - az * by, nrm[1] = az * bx - ax * bz, nrm[2] = ax * by - ay * bx;
                normalize(nrm);
                Builder::Part& part = b.parts[s.texture];
                const uint32_t first = uint32_t(part.vertices.size());
                for (int k = 0; k < 4; ++k) {
                    Vertex v{};
                    std::copy(p[k], p[k] + 3, v.pos);
                    v.plain = v.pos[1];  // a wall: as it is
                    std::copy(nrm, nrm + 3, v.normal);
                    v.uv[0] = s.u[k];
                    v.uv[1] = s.v[k];
                    b.lightmapUv(s.lightmap, float(k & 1), float(k >> 1), v.lm);
                    v.color = s.color;
                    part.vertices.push_back(v);
                }
                Builder::quad(part, first, first + 1, first + 2, first + 3);
            };
            for (int y = cy0; y < cy1; ++y)
                for (int x = cx0; x < cx1; ++x) {
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
            if (b.parts.empty()) continue;
            if (n > 1)  // 3.0.4-beta.12: the strips between the cells that look different
                for (int y = cy0; y < cy1; ++y)
                    for (int x = cx0; x < cx1; ++x) blendsOf(b, grid, x, y, n, laid, resting);
            Chunk ch;
            ch.cellX = cx0;
            ch.cellY = cy0;
            ch.firstVertex = uint32_t(m.vertices.size());
            ch.firstBatch = uint32_t(m.batches.size());
            for (int k = 0; k < 3; ++k) ch.lo[k] = 1e30f, ch.hi[k] = -1e30f;
            const auto put = [&](const std::map<int, Builder::Part>& parts, uint8_t layer) {
                for (const auto& t : parts) {
                    Batch batch;
                    batch.texture = t.first;
                    batch.layer = layer;
                    batch.firstVertex = uint32_t(m.vertices.size());
                    batch.vertexCount = uint32_t(t.second.vertices.size());
                    batch.firstIndex = uint32_t(m.indices.size());
                    batch.indexCount = uint32_t(t.second.indices.size());
                    for (const Vertex& v : t.second.vertices)
                        for (int k = 0; k < 3; ++k)
                            ch.lo[k] = std::min(ch.lo[k], v.pos[k]), ch.hi[k] = std::max(ch.hi[k], v.pos[k]);
                    m.vertices.insert(m.vertices.end(), t.second.vertices.begin(), t.second.vertices.end());
                    m.indices.insert(m.indices.end(), t.second.indices.begin(), t.second.indices.end());
                    m.batches.push_back(batch);
                }
            };
            put(b.parts, kGround);
            put(b.bands, kOwnBand);
            put(b.strips, kNeighbour);
            ch.vertexCount = uint32_t(m.vertices.size()) - ch.firstVertex;
            ch.batchCount = uint32_t(m.batches.size()) - ch.firstBatch;
            m.chunks.push_back(ch);
        }
    return m;
}

std::vector<uint8_t> restingOn(const gnd::Ground& g, const Mesh& models) {
    std::vector<uint8_t> out(g.cells.size(), 0);
    const float half[2] = {float(g.width) / 2.0f, float(g.height) / 2.0f};
    static const float kAt[3] = {0.15f, 0.5f, 0.85f};  // each cell looked at in 3 x 3 places
    // a batch's indices count from its first vertex (terrain::Batch)
    std::vector<std::pair<size_t, uint32_t>> triangles;  // each triangle's first index, its batch's first vertex
    for (const Batch& b : models.batches)
        for (uint32_t i = 0; i + 2 < b.indexCount; i += 3)
            triangles.emplace_back(size_t(b.firstIndex) + i, b.firstVertex);
    for (const auto& [t, base] : triangles) {
        const float* p[3] = {nullptr, nullptr, nullptr};
        bool bad = t + 2 >= models.indices.size();
        for (int k = 0; k < 3 && !bad; ++k) {
            const size_t i = size_t(base) + models.indices[t + size_t(k)];
            bad = i >= models.vertices.size();
            if (!bad) p[k] = models.vertices[i].pos;
        }
        if (bad) continue;
        const float d = (p[1][2] - p[2][2]) * (p[0][0] - p[2][0]) + (p[2][0] - p[1][0]) * (p[0][2] - p[2][2]);
        if (std::fabs(d) < 1e-6f) continue;  // upright: seen edge-on from above
        const float lo[2] = {std::min({p[0][0], p[1][0], p[2][0]}), std::min({p[0][2], p[1][2], p[2][2]})};
        const float hi[2] = {std::max({p[0][0], p[1][0], p[2][0]}), std::max({p[0][2], p[1][2], p[2][2]})};
        const int x0 = std::max(0, int(std::floor(lo[0] / g.zoom + half[0])));
        const int x1 = std::min(g.width - 1, int(std::floor(hi[0] / g.zoom + half[0])));
        const int y0 = std::max(0, int(std::floor(lo[1] / g.zoom + half[1])));
        const int y1 = std::min(g.height - 1, int(std::floor(hi[1] / g.zoom + half[1])));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const size_t at = size_t(y) * size_t(g.width) + size_t(x);
                const gnd::Cell& c = g.cells[at];
                if (out[at] || c.top < 0) continue;
                for (int s = 0; s < 9 && !out[at]; ++s) {
                    const float fu = kAt[s % 3], fv = kAt[s / 3];
                    const float wx = (float(x) + fu - half[0]) * g.zoom, wz = (float(y) + fv - half[1]) * g.zoom;
                    const float a = ((p[1][2] - p[2][2]) * (wx - p[2][0]) + (p[2][0] - p[1][0]) * (wz - p[2][2])) / d;
                    const float b = ((p[2][2] - p[0][2]) * (wx - p[2][0]) + (p[0][0] - p[2][0]) * (wz - p[2][2])) / d;
                    if (a < 0.0f || b < 0.0f || a + b > 1.0f) continue;
                    const float face = a * p[0][1] + b * p[1][1] + (1.0f - a - b) * p[2][1];
                    const float plain = plainAt(c, fu, fv);
                    out[at] = face >= plain - kRestReach && face <= plain + kRestUnder;  // up along -y
                }
            }
    }
    return out;
}

Mesh water(const gnd::Ground& g, float level, float waveHeight) {
    const float half[2] = {float(g.width) / 2.0f, float(g.height) / 2.0f};
    const float highest = level - waveHeight;
    Mesh m;
    for (int cy0 = 0; cy0 < g.height; cy0 += kChunk)
        for (int cx0 = 0; cx0 < g.width; cx0 += kChunk) {
            Chunk ch;
            ch.cellX = cx0, ch.cellY = cy0;
            ch.firstVertex = uint32_t(m.vertices.size());
            ch.firstBatch = uint32_t(m.batches.size());
            Batch b;
            b.texture = 0;
            b.firstVertex = ch.firstVertex;
            b.firstIndex = uint32_t(m.indices.size());
            const int cx1 = std::min(cx0 + kChunk, g.width), cy1 = std::min(cy0 + kChunk, g.height);
            for (int y = cy0; y < cy1; ++y)
                for (int x = cx0; x < cx1; ++x) {
                    const gnd::Cell& c = g.cells[size_t(y) * size_t(g.width) + size_t(x)];
                    if (c.top < 0 || !(c.h[0] > highest || c.h[1] > highest || c.h[2] > highest || c.h[3] > highest))
                        continue;
                    const uint32_t first = uint32_t(m.vertices.size()) - b.firstVertex;
                    for (int k = 0; k < 4; ++k) {  // (x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1) as the game's
                        const int du = k & 1, dv = k >> 1;
                        Vertex v{};
                        v.pos[0] = (float(x + du) - half[0]) * g.zoom;
                        v.pos[1] = level;
                        v.pos[2] = (float(y + dv) - half[1]) * g.zoom;
                        v.plain = level;
                        v.normal[1] = -1.0f;
                        v.uv[0] = float((x & 3) + du) * 0.25f;
                        v.uv[1] = float((y & 3) + dv) * 0.25f;
                        v.color = 0x90FFFFFFu;
                        m.vertices.push_back(v);
                    }
                    for (uint32_t k : {0u, 1u, 2u, 2u, 1u, 3u}) m.indices.push_back(first + k);
                }
            b.vertexCount = uint32_t(m.vertices.size()) - b.firstVertex;
            b.indexCount = uint32_t(m.indices.size()) - b.firstIndex;
            if (!b.vertexCount) continue;
            ch.vertexCount = b.vertexCount;
            ch.batchCount = 1;
            for (int a = 0; a < 3; ++a) ch.lo[a] = 1e30f, ch.hi[a] = -1e30f;
            for (uint32_t k = b.firstVertex; k < b.firstVertex + b.vertexCount; ++k)
                for (int a = 0; a < 3; ++a) {
                    ch.lo[a] = std::min(ch.lo[a], m.vertices[k].pos[a]);
                    ch.hi[a] = std::max(ch.hi[a], m.vertices[k].pos[a]);
                }
            m.batches.push_back(b);
            m.chunks.push_back(ch);
        }
    return m;
}

}  // namespace uagfx::terrain
