#include "core/seams.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace uagfx::seams {
namespace {

// A cell: its four corners - 0 (x0, z0), 1 (x1, z0), 2 (x0, z1), 3 (x1, z1) - and its group.
struct Cell {
    Vertex corner[4];
    bool have[4] = {false, false, false, false};
    uint32_t group = 0;
};

int64_t keyOf(int x, int z) { return (int64_t(x) << 32) ^ int64_t(uint32_t(z)); }

uint32_t lerpColour(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) {
        const float ca = float((a >> s) & 255), cb = float((b >> s) & 255);
        out |= uint32_t(std::clamp(ca + (cb - ca) * t + 0.5f, 0.0f, 255.0f)) << s;
    }
    return out;
}

Vertex lerp(const Vertex& a, const Vertex& b, float t) {
    Vertex v;
    for (int k = 0; k < 3; ++k) v.pos[k] = a.pos[k] + (b.pos[k] - a.pos[k]) * t;
    for (int k = 0; k < 2; ++k) {
        v.uv0[k] = a.uv0[k] + (b.uv0[k] - a.uv0[k]) * t;
        v.uv1[k] = a.uv1[k] + (b.uv1[k] - a.uv1[k]) * t;
    }
    v.diffuse = lerpColour(a.diffuse, b.diffuse, t);
    v.specular = lerpColour(a.specular, b.specular, t);
    return v;
}

// The cell's attributes at (u, v) of its own square - bilinear; outside [0, 1] carried on.
Vertex at(const Cell& c, float u, float v) {
    const Vertex top = lerp(c.corner[0], c.corner[1], u), bottom = lerp(c.corner[2], c.corner[3], u);
    return lerp(top, bottom, v);
}

bool same(float a, float b) { return std::fabs(a - b) < 0.02f; }

}  // namespace

float cellSide(const std::vector<Triangle>& ground) {
    std::vector<float> sides;
    for (const Triangle& t : ground) {
        if (std::fabs(t.v[0].pos[1] - t.v[1].pos[1]) > 4.0f || std::fabs(t.v[0].pos[1] - t.v[2].pos[1]) > 4.0f)
            continue;  // a wall
        float x0 = t.v[0].pos[0], x1 = x0, z0 = t.v[0].pos[2], z1 = z0;
        for (const Vertex& v : t.v) {
            x0 = std::min(x0, v.pos[0]);
            x1 = std::max(x1, v.pos[0]);
            z0 = std::min(z0, v.pos[2]);
            z1 = std::max(z1, v.pos[2]);
        }
        sides.push_back(std::max(x1 - x0, z1 - z0));
        if (sides.size() >= 256) break;
    }
    if (sides.empty()) return 0.0f;
    std::nth_element(sides.begin(), sides.begin() + std::ptrdiff_t(sides.size() / 2), sides.end());
    return sides[sides.size() / 2];
}

std::vector<Strip> build(const std::vector<Triangle>& ground, const std::vector<uint32_t>& texture, float width) {
    std::vector<Strip> out;
    const float side = cellSide(ground);
    if (side < 0.5f || ground.empty()) return out;
    // The grid's origin: the cells' corners lie on it.
    const float ox = ground[0].v[0].pos[0] - std::floor(ground[0].v[0].pos[0] / side) * side;
    const float oz = ground[0].v[0].pos[2] - std::floor(ground[0].v[0].pos[2] / side) * side;
    std::unordered_map<int64_t, Cell> cells;
    for (const Triangle& t : ground) {
        // the ground's own faces only: its walls (the cliffs' sides) stand up
        const float ax = t.v[1].pos[0] - t.v[0].pos[0], ay = t.v[1].pos[1] - t.v[0].pos[1],
                    az = t.v[1].pos[2] - t.v[0].pos[2], bx = t.v[2].pos[0] - t.v[0].pos[0],
                    by = t.v[2].pos[1] - t.v[0].pos[1], bz = t.v[2].pos[2] - t.v[0].pos[2];
        const float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        if (std::fabs(ny) < 0.6f * std::sqrt(nx * nx + ny * ny + nz * nz)) continue;
        const float cx = (t.v[0].pos[0] + t.v[1].pos[0] + t.v[2].pos[0]) / 3.0f;
        const float cz = (t.v[0].pos[2] + t.v[1].pos[2] + t.v[2].pos[2]) / 3.0f;
        const int ix = int(std::floor((cx - ox) / side)), iz = int(std::floor((cz - oz) / side));
        Cell& c = cells[keyOf(ix, iz)];
        c.group = t.group;
        const float x0 = ox + float(ix) * side, z0 = oz + float(iz) * side;
        for (const Vertex& v : t.v) {
            const int cx1 = std::fabs(v.pos[0] - x0) < std::fabs(v.pos[0] - x0 - side) ? 0 : 1;
            const int cz1 = std::fabs(v.pos[2] - z0) < std::fabs(v.pos[2] - z0 - side) ? 0 : 1;
            const int k = cz1 * 2 + cx1;
            c.corner[k] = v;
            c.have[k] = true;
        }
    }
    std::unordered_map<uint64_t, size_t> strips;  // (inside, shows) -> its strip
    // The four neighbours: the direction in the grid, this cell's corners on the shared edge (in the order along
    // it) and the neighbour's same corners, and (u, v) of a point at (along, into) the cell from the edge.
    struct Way {
        int dx, dz;
        int mine[2], theirs[2];
    };
    static const Way kWays[4] = {{1, 0, {1, 3}, {0, 2}}, {-1, 0, {0, 2}, {1, 3}},
                                 {0, 1, {2, 3}, {0, 1}}, {0, -1, {0, 1}, {2, 3}}};
    constexpr int kAlong = 4, kAcross = 2;
    for (const auto& entry : cells) {
        const Cell& a = entry.second;
        if (!(a.have[0] && a.have[1] && a.have[2] && a.have[3])) continue;
        const int ix = int(entry.first >> 32), iz = int(int32_t(uint32_t(entry.first)));
        for (const Way& w : kWays) {
            const auto nb = cells.find(keyOf(ix + w.dx, iz + w.dz));
            if (nb == cells.end()) continue;
            const Cell& b = nb->second;
            if (!(b.have[0] && b.have[1] && b.have[2] && b.have[3])) continue;
            const uint32_t ta = a.group < texture.size() ? texture[a.group] : 0;
            const uint32_t tb = b.group < texture.size() ? texture[b.group] : 0;
            if (ta == tb) {  // one texture: a seam only where its coordinates jump at the edge
                bool alike = true;
                for (int k = 0; k < 2; ++k) {
                    const Vertex& m = a.corner[w.mine[k]];
                    const Vertex& t = b.corner[w.theirs[k]];
                    alike = alike && same(m.uv0[0], t.uv0[0]) && same(m.uv0[1], t.uv0[1]);
                }
                if (alike) continue;
            }
            const uint64_t id = uint64_t(a.group) << 32 | b.group;
            auto found = strips.find(id);
            if (found == strips.end()) {
                found = strips.emplace(id, out.size()).first;
                Strip s;
                s.inside = a.group;
                s.shows = b.group;
                out.push_back(s);
            }
            Strip& s = out[found->second];
            if (s.vertices.size() + (kAlong + 1) * (kAcross + 1) > 65535) continue;
            const uint16_t first = uint16_t(s.vertices.size());
            const bool low = entry.first < nb->first;  // the side that shows the other where the noise is low
            for (int j = 0; j <= kAcross; ++j)
                for (int i = 0; i <= kAlong; ++i) {
                    const float along = float(i) / kAlong, into = width * float(j) / kAcross;
                    // (u, v) in this cell and in the neighbour's, for the point `into` from the shared edge
                    float u = 0, v = 0;
                    if (w.dx == 1) { u = 1.0f - into; v = along; }
                    if (w.dx == -1) { u = into; v = along; }
                    if (w.dz == 1) { u = along; v = 1.0f - into; }
                    if (w.dz == -1) { u = along; v = into; }
                    Vertex p = at(a, u, v);
                    const Vertex q = at(b, u - float(w.dx), v - float(w.dz));
                    p.uv0[0] = q.uv0[0];
                    p.uv0[1] = q.uv0[1];
                    const uint32_t edge = uint32_t(255.0f * (1.0f - float(j) / kAcross) + 0.5f);
                    p.diffuse = (p.diffuse & 0x00ffffffu) | edge << 24;
                    p.specular = (p.specular & 0xffff00ffu) | (low ? 0xff00u : 0u);
                    s.vertices.push_back(p);
                }
            for (int j = 0; j < kAcross; ++j)
                for (int i = 0; i < kAlong; ++i) {
                    const uint16_t q0 = uint16_t(first + j * (kAlong + 1) + i), q1 = uint16_t(q0 + 1),
                                   q2 = uint16_t(q0 + kAlong + 1), q3 = uint16_t(q2 + 1);
                    for (uint16_t k : {q0, q1, q2, q2, q1, q3}) s.indices.push_back(k);
                }
        }
    }
    return out;
}

}  // namespace uagfx::seams
