#include "core/ground_cells.hpp"

#include <algorithm>
#include <cmath>

#include "core/round.hpp"

// 3.0.5-beta.9: floorInt for int(std::floor(...)) - the same answers without the 32-bit build's x87 library call, six a
// ground triangle each frame (R-VMPQKX)
namespace uagfx::ground_cells {

Grid build(const std::vector<seams::Triangle>& ground) {
    Grid g;
    const float side = seams::cellSide(ground);
    if (side < 0.5f || ground.empty()) return g;
    // the cells' corners lie on the grid of the first vertex (as seams::build's)
    const float ox = ground[0].v[0].pos[0] - std::floor(ground[0].v[0].pos[0] / side) * side;
    const float oz = ground[0].v[0].pos[2] - std::floor(ground[0].v[0].pos[2] / side) * side;
    double mx = 0.0, mz = 0.0;
    for (const seams::Triangle& t : ground) {
        mx += (t.v[0].pos[0] + t.v[1].pos[0] + t.v[2].pos[0]) / 3.0;
        mz += (t.v[0].pos[2] + t.v[1].pos[2] + t.v[2].pos[2]) / 3.0;
    }
    mx /= double(ground.size());
    mz /= double(ground.size());
    int x0 = 1 << 30, z0 = 1 << 30, x1 = -(1 << 30), z1 = -(1 << 30);
    for (const seams::Triangle& t : ground) {
        const float cx = (t.v[0].pos[0] + t.v[1].pos[0] + t.v[2].pos[0]) / 3.0f;
        const float cz = (t.v[0].pos[2] + t.v[1].pos[2] + t.v[2].pos[2]) / 3.0f;
        x0 = std::min(x0, floorInt((cx - ox) / side));
        x1 = std::max(x1, floorInt((cx - ox) / side));
        z0 = std::min(z0, floorInt((cz - oz) / side));
        z1 = std::max(z1, floorInt((cz - oz) / side));
    }
    // at most kMostSide a side, about the middle
    const int midX = floorInt((float(mx) - ox) / side), midZ = floorInt((float(mz) - oz) / side);
    x0 = std::max(x0, midX - kMostSide / 2);
    z0 = std::max(z0, midZ - kMostSide / 2);
    x1 = std::min(x1, x0 + kMostSide - 1);
    z1 = std::min(z1, z0 + kMostSide - 1);
    if (x1 < x0 || z1 < z0) return g;
    g.side = side;
    g.originX = ox + float(x0) * side;
    g.originZ = oz + float(z0) * side;
    g.width = x1 - x0 + 1;
    g.height = z1 - z0 + 1;
    g.span.assign(size_t(g.width) * size_t(g.height) * 2, 0.0f);
    for (size_t i = 0; i < g.span.size(); i += 2) {
        g.span[i] = 1e30f;
        g.span[i + 1] = -1e30f;
    }
    for (const seams::Triangle& t : ground) {
        // the ground's own faces only: the cliffs' walls stand up between two heights
        const float ax = t.v[1].pos[0] - t.v[0].pos[0], ay = t.v[1].pos[1] - t.v[0].pos[1],
                    az = t.v[1].pos[2] - t.v[0].pos[2], bx = t.v[2].pos[0] - t.v[0].pos[0],
                    by = t.v[2].pos[1] - t.v[0].pos[1], bz = t.v[2].pos[2] - t.v[0].pos[2];
        const float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        if (std::fabs(ny) < 0.6f * std::sqrt(nx * nx + ny * ny + nz * nz)) continue;
        const float cx = (t.v[0].pos[0] + t.v[1].pos[0] + t.v[2].pos[0]) / 3.0f;
        const float cz = (t.v[0].pos[2] + t.v[1].pos[2] + t.v[2].pos[2]) / 3.0f;
        const int ix = floorInt((cx - g.originX) / side), iz = floorInt((cz - g.originZ) / side);
        if (ix < 0 || iz < 0 || ix >= g.width || iz >= g.height) continue;
        float* s = &g.span[(size_t(iz) * size_t(g.width) + size_t(ix)) * 2];
        for (const seams::Vertex& v : t.v) {
            s[0] = std::min(s[0], v.pos[1]);
            s[1] = std::max(s[1], v.pos[1]);
        }
    }
    return g;
}

bool onGround(const Grid& g, float x, float y, float z, float slack) {
    if (g.side <= 0.0f) return true;
    const int ix = floorInt((x - g.originX) / g.side), iz = floorInt((z - g.originZ) / g.side);
    if (ix < 0 || iz < 0 || ix >= g.width || iz >= g.height) return true;
    const float* s = &g.span[(size_t(iz) * size_t(g.width) + size_t(ix)) * 2];
    return s[0] <= s[1] && y >= s[0] - slack && y <= s[1] + slack;
}

}  // namespace uagfx::ground_cells
