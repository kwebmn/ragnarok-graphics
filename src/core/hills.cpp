#include "core/hills.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace uagfx::hills {

float pack(float nx, float nz) {
    const float a = 1.0f + std::round((std::clamp(nx, -1.0f, 1.0f) * 0.5f + 0.5f) * 2046.0f);
    const float b = std::round((std::clamp(nz, -1.0f, 1.0f) * 0.5f + 0.5f) * 4095.0f);
    return a * 4096.0f + b;
}

bool unpack(float v, float& nx, float& ny, float& nz) {
    if (v < 4096.0f) return false;
    const float a = std::floor(v / 4096.0f), b = v - a * 4096.0f;
    nx = (a - 1.0f) / 2046.0f * 2.0f - 1.0f;
    nz = b / 4095.0f * 2.0f - 1.0f;
    ny = -std::sqrt(std::max(0.0f, 1.0f - nx * nx - nz * nz));
    return true;
}

Grid build(const std::vector<seams::Triangle>& ground) {
    Grid g;
    const float side = seams::cellSide(ground);
    if (side < 0.5f || ground.empty()) return g;
    const float px = ground[0].v[0].pos[0] - std::floor(ground[0].v[0].pos[0] / side) * side;
    const float pz = ground[0].v[0].pos[2] - std::floor(ground[0].v[0].pos[2] / side) * side;
    struct Sum {
        double x = 0, y = 0, z = 0;
    };
    std::unordered_map<int64_t, Sum> corners;
    const auto keyOf = [](int ix, int iz) { return int64_t(ix) << 32 | int64_t(uint32_t(iz)); };
    int x0 = 1 << 30, x1 = -(1 << 30), z0 = 1 << 30, z1 = -(1 << 30);
    double mx = 0, mz = 0;
    size_t faces = 0;
    for (const seams::Triangle& t : ground) {
        const float ax = t.v[1].pos[0] - t.v[0].pos[0], ay = t.v[1].pos[1] - t.v[0].pos[1],
                    az = t.v[1].pos[2] - t.v[0].pos[2];
        const float bx = t.v[2].pos[0] - t.v[0].pos[0], by = t.v[2].pos[1] - t.v[0].pos[1],
                    bz = t.v[2].pos[2] - t.v[0].pos[2];
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len < 1e-6f || std::fabs(ny) < 0.5f * len) continue;  // a wall of a cliff, or nothing
        if (ny > 0.0f) nx = -nx, ny = -ny, nz = -nz;              // upwards (-y); its length its area's twice
        for (const seams::Vertex& v : t.v) {
            const int ix = int(std::lround((v.pos[0] - px) / side)), iz = int(std::lround((v.pos[2] - pz) / side));
            Sum& s = corners[keyOf(ix, iz)];
            s.x += nx;
            s.y += ny;
            s.z += nz;
            x0 = std::min(x0, ix);
            x1 = std::max(x1, ix);
            z0 = std::min(z0, iz);
            z1 = std::max(z1, iz);
            mx += ix;
            mz += iz;
            ++faces;
        }
    }
    if (corners.empty()) return g;
    // at most kMostSide a side, about the corners' middle
    const int cx = int(std::lround(mx / double(faces))), cz = int(std::lround(mz / double(faces)));
    x0 = std::max(x0, cx - kMostSide / 2);
    z0 = std::max(z0, cz - kMostSide / 2);
    x1 = std::min(x1, x0 + kMostSide - 1);
    z1 = std::min(z1, z0 + kMostSide - 1);
    g.side = side;
    g.width = x1 - x0 + 1;
    g.height = z1 - z0 + 1;
    g.originX = px + float(x0) * side;
    g.originZ = pz + float(z0) * side;
    std::vector<Sum> raw(size_t(g.width) * size_t(g.height));
    std::vector<bool> have(raw.size(), false);
    for (const auto& [key, s] : corners) {
        const int ix = int(key >> 32) - x0, iz = int(int32_t(uint32_t(key))) - z0;
        if (ix < 0 || iz < 0 || ix >= g.width || iz >= g.height) continue;
        const size_t at = size_t(iz) * size_t(g.width) + size_t(ix);
        const double l = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
        if (l < 1e-9) continue;
        raw[at] = Sum{s.x / l, s.y / l, s.z / l};
        have[at] = true;
    }
    // once over the neighbours (the corner itself counting twice): the hills' light rounder still
    g.packed.assign(raw.size(), 0.0f);
    for (int iz = 0; iz < g.height; ++iz)
        for (int ix = 0; ix < g.width; ++ix) {
            const size_t at = size_t(iz) * size_t(g.width) + size_t(ix);
            if (!have[at]) continue;
            Sum s{raw[at].x, raw[at].y, raw[at].z};
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int qx = ix + dx, qz = iz + dz;
                    if (qx < 0 || qz < 0 || qx >= g.width || qz >= g.height) continue;
                    const size_t q = size_t(qz) * size_t(g.width) + size_t(qx);
                    if (!have[q]) continue;
                    s.x += raw[q].x;
                    s.y += raw[q].y;
                    s.z += raw[q].z;
                }
            const double l = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
            g.packed[at] = pack(float(s.x / l), float(s.z / l));
        }
    return g;
}

}  // namespace uagfx::hills
