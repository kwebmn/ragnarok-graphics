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

Grid ofMeshes(const std::vector<const terrain::Mesh*>& meshes, int width, int height, float zoom, int signX, int signY,
              int signZ) {
    Grid g;
    if (meshes.empty() || width <= 0 || height <= 0 || zoom < 0.5f) return g;
    // at most kMostSide a side, about the map's middle
    const int w = std::min(width, kMostSide), h = std::min(height, kMostSide);
    const int x0 = (width - w) / 2, z0 = (height - h) / 2;  // the first of the file's cells kept
    g.side = zoom;
    g.width = w;
    g.height = h;
    g.originX = -0.5f * float(w) * zoom;  // the map's middle at 0 either way round
    g.originZ = -0.5f * float(h) * zoom;
    g.span.assign(size_t(w) * size_t(h) * 2, 0.0f);
    for (size_t i = 0; i < g.span.size(); i += 2) {
        g.span[i] = 1e30f;
        g.span[i + 1] = -1e30f;
    }
    const float up = signY < 0 ? -1.0f : 1.0f;
    for (const terrain::Mesh* mesh : meshes) {
        if (!mesh) continue;
        for (const terrain::Batch& b : mesh->batches) {
            if (b.layer != terrain::kGround) continue;
            for (uint32_t i = b.firstIndex; i + 3 <= b.firstIndex + b.indexCount; i += 3) {
                const terrain::Vertex* v[3];
                bool ok = true;
                for (int k = 0; k < 3 && ok; ++k) {
                    const size_t at = size_t(b.firstVertex) + mesh->indices[i + size_t(k)];
                    ok = at < mesh->vertices.size();
                    if (ok) v[k] = &mesh->vertices[at];
                }
                // a wall's (terrain::isWall: also one with no face at all, two corners at one place)
                if (!ok || terrain::isWall(*v[0]) || terrain::isWall(*v[1]) || terrain::isWall(*v[2])) continue;
                // the ground's own faces only: the cliffs' walls stand up between two heights (as build's)
                const float ax = v[1]->pos[0] - v[0]->pos[0], ay = v[1]->pos[1] - v[0]->pos[1],
                            az = v[1]->pos[2] - v[0]->pos[2], bx = v[2]->pos[0] - v[0]->pos[0],
                            by = v[2]->pos[1] - v[0]->pos[1], bz = v[2]->pos[2] - v[0]->pos[2];
                const float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
                const float nn = nx * nx + ny * ny + nz * nz;
                if (nn < 1e-12f || std::fabs(ny) < 0.6f * std::sqrt(nn)) continue;
                const float cx = (v[0]->pos[0] + v[1]->pos[0] + v[2]->pos[0]) / 3.0f;
                const float cz = (v[0]->pos[2] + v[1]->pos[2] + v[2]->pos[2]) / 3.0f;
                int ix = floorInt(cx / zoom + 0.5f * float(width)) - x0;
                int iz = floorInt(cz / zoom + 0.5f * float(height)) - z0;
                if (ix < 0 || iz < 0 || ix >= w || iz >= h) continue;
                if (signX < 0) ix = w - 1 - ix;
                if (signZ < 0) iz = h - 1 - iz;
                float* s = &g.span[(size_t(iz) * size_t(w) + size_t(ix)) * 2];
                for (const terrain::Vertex* p : v)
                    for (const float y : {p->pos[1] * up, p->plain * up}) {
                        s[0] = std::min(s[0], y);
                        s[1] = std::max(s[1], y);
                    }
            }
        }
    }
    return g;
}

namespace {

// the shaders' bladeGreen (grass3d_vs, grass_ps): how green a colour is by the grass's measure, 0-1
float greenOf(uint32_t argb) {
    const float r = float((argb >> 16) & 0xFF) / 255.0f, gr = float((argb >> 8) & 0xFF) / 255.0f,
                b = float(argb & 0xFF) / 255.0f;
    const float a = std::min((gr / std::max(r, 1e-4f) - 0.96f) / 0.05f, (gr / std::max(b, 1e-4f) - 1.08f) / 0.08f);
    return std::clamp(std::min(a, (gr - 0.08f) / 0.04f), 0.0f, 1.0f);
}

}  // namespace

float greenOfCell(const gnd::Ground& g, size_t cell, const std::vector<Picture>& pictures) {
    if (cell >= g.cells.size()) return -1.0f;
    const gnd::Cell& c = g.cells[cell];
    if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) return -1.0f;
    const gnd::Surface& s = g.surfaces[size_t(c.top)];
    if (s.texture < 0 || size_t(s.texture) >= pictures.size()) return -1.0f;
    const Picture& p = pictures[size_t(s.texture)];
    if (!p.argb || p.width <= 0 || p.height <= 0) return -1.0f;
    constexpr int kAcross = 8;  // its picture's part looked at so many times each way
    const float u0 = std::min({s.u[0], s.u[1], s.u[2], s.u[3]}), u1 = std::max({s.u[0], s.u[1], s.u[2], s.u[3]});
    const float v0 = std::min({s.v[0], s.v[1], s.v[2], s.v[3]}), v1 = std::max({s.v[0], s.v[1], s.v[2], s.v[3]});
    float sum = 0.0f;
    for (int j = 0; j < kAcross; ++j)
        for (int k = 0; k < kAcross; ++k) {
            const float u = u0 + (u1 - u0) * (float(k) + 0.5f) / float(kAcross);
            const float v = v0 + (v1 - v0) * (float(j) + 0.5f) / float(kAcross);
            const int x = std::clamp(floorInt(u * float(p.width)), 0, p.width - 1);
            const int y = std::clamp(floorInt(v * float(p.height)), 0, p.height - 1);
            sum += greenOf(p.argb[size_t(y) * size_t(p.width) + size_t(x)]);
        }
    return sum / float(kAcross * kAcross);
}

std::vector<uint8_t> grassyCells(const gnd::Ground& g, const std::vector<Picture>& pictures) {
    const int w = std::max(g.width, 0), h = std::max(g.height, 0);
    std::vector<uint8_t> out(size_t(w) * size_t(h), 1);
    if (g.cells.size() < out.size()) return out;
    for (size_t i = 0; i < out.size(); ++i) {
        const float green = greenOfCell(g, i, pictures);
        out[i] = green < 0.0f || green >= kGrassy ? 1 : 0;
    }
    return out;
}

std::vector<uint8_t> grassyCells(const gnd::Ground& g, const PictureOf& pictureOf) {
    const int w = std::max(g.width, 0), h = std::max(g.height, 0);
    std::vector<uint8_t> out(size_t(w) * size_t(h), 1);
    if (g.cells.size() < out.size() || !pictureOf) return out;
    std::vector<std::vector<uint32_t>> cellsOf(g.textures.size());  // the cells whose tops wear each texture
    for (size_t i = 0; i < out.size(); ++i) {
        const gnd::Cell& c = g.cells[i];
        if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) continue;
        const int texture = g.surfaces[size_t(c.top)].texture;
        if (texture >= 0 && size_t(texture) < cellsOf.size()) cellsOf[size_t(texture)].push_back(uint32_t(i));
    }
    std::vector<Picture> one(g.textures.size());  // the one known at the time
    for (size_t t = 0; t < cellsOf.size(); ++t) {
        Picture p;
        if (cellsOf[t].empty() || !pictureOf(t, p)) continue;
        one[t] = p;
        for (const uint32_t i : cellsOf[t]) {
            const float green = greenOfCell(g, i, one);
            out[i] = green < 0.0f || green >= kGrassy ? 1 : 0;
        }
        one[t] = Picture{};
    }
    return out;
}

size_t dropWhere(Grid& g, const std::function<bool(float x, float z)>& off) {
    if (g.side <= 0.0f || g.width <= 0 || g.height <= 0 || g.span.size() < size_t(g.width) * size_t(g.height) * 2)
        return 0;
    size_t dropped = 0;
    for (int z = 0; z < g.height; ++z)
        for (int x = 0; x < g.width; ++x) {
            float* s = &g.span[(size_t(z) * size_t(g.width) + size_t(x)) * 2];
            if (s[0] > s[1]) continue;  // no ground there already
            if (!off(g.originX + (float(x) + 0.5f) * g.side, g.originZ + (float(z) + 0.5f) * g.side)) continue;
            s[0] = 1e30f;
            s[1] = -1e30f;
            ++dropped;
        }
    return dropped;
}

}  // namespace uagfx::ground_cells
