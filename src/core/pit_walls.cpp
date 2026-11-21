#include "core/pit_walls.hpp"

#include "core/outdoors.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "core/outskirts.hpp"

namespace uagfx::pitwalls {
namespace {

struct Part {
    std::vector<terrain::Vertex> vertices;
    std::vector<uint32_t> indices;
};

bool fillerOf(const gnd::Ground& g, int texture) {
    return texture >= 0 && size_t(texture) < g.textures.size() && outskirts::fillerTexture(g.textures[size_t(texture)]);
}

// a pit: no top, or the makers' filler on top
bool pit(const gnd::Ground& g, const gnd::Cell& c) {
    if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) return true;
    return fillerOf(g, g.surfaces[size_t(c.top)].texture);
}

const gnd::Surface* surfaceAt(const gnd::Ground& g, int index) {
    return index >= 0 && size_t(index) < g.surfaces.size() && g.surfaces[size_t(index)].texture >= 0
               ? &g.surfaces[size_t(index)]
               : nullptr;
}

// the light the map's sun gives a face of the game's normal n (terrain::lit of white), its green
float lightOf(const terrain::Sun& sun, const float n[3]) {
    return float((terrain::lit(0xFFFFFFFFu, sun, n) >> 8) & 0xFFu) / 255.0f;
}

// one side of a floor cell (x, y) to its neighbour: dx, dy the way; the floor's two corners on that edge (0-3, the
// surface's order) and the neighbour's facing them; the side's surface and whether the floor holds it
struct Side {
    int dx, dy;
    int floorA, floorB;  // the floor's corners along the edge
    int pitA, pitB;      // the neighbour's, beside them
};
constexpr Side kSides[4] = {
    {1, 0, 3, 1, 2, 0},   // its right side (x + 1): corners (x + 1, y + 1), (x + 1, y)
    {-1, 0, 2, 0, 3, 1},  // its left (x): (x, y + 1), (x, y)
    {0, 1, 2, 3, 0, 1},   // its far side (y + 1): (x, y + 1), (x + 1, y + 1)
    {0, -1, 0, 1, 2, 3},  // its near side (y): (x, y), (x + 1, y)
};

}  // namespace

bool wallsFor(const std::string& map) { return !map.empty() && !outdoors(map); }

terrain::Mesh build(const gnd::Ground& g, const terrain::Atlas& atlas, const terrain::Sun& sun, float height) {
    terrain::Mesh m;
    if (g.width <= 0 || g.height <= 0 || g.cells.size() < size_t(g.width) * size_t(g.height)) return m;
    // the map's commonest side picture (not the filler's)
    std::map<int, size_t> sides;
    for (const gnd::Cell& c : g.cells)
        for (const int s : {c.front, c.right})
            if (const gnd::Surface* sf = surfaceAt(g, s); sf && !fillerOf(g, sf->texture)) ++sides[sf->texture];
    int commonest = -1;
    size_t most = 0;
    for (const auto& [texture, count] : sides)
        if (count > most) most = count, commonest = texture;
    const float halfW = 0.5f * float(g.width), halfH = 0.5f * float(g.height);
    const float intoGround[3] = {0.0f, 1.0f, 0.0f};  // the game's normal of a top (terrain::lit)
    const float floorLight = std::max(lightOf(sun, intoGround), 1e-3f);
    const auto cellAt = [&](int x, int y) -> const gnd::Cell& {
        return g.cells[size_t(y) * size_t(g.width) + size_t(x)];
    };
    // a corner (0-3) of cell (x, y) in the world (the maps' axes x +1, z +1; up along -y)
    const auto cornerXZ = [&](int x, int y, int k, float& wx, float& wz) {
        wx = (float(x + (k & 1)) - halfW) * g.zoom;
        wz = (float(y + (k >> 1)) - halfH) * g.zoom;
    };
    for (int cy0 = 0; cy0 < g.height; cy0 += terrain::kChunk)
        for (int cx0 = 0; cx0 < g.width; cx0 += terrain::kChunk) {
            std::map<int, Part> parts;
            const int cy1 = std::min(cy0 + terrain::kChunk, g.height), cx1 = std::min(cx0 + terrain::kChunk, g.width);
            for (int y = cy0; y < cy1; ++y)
                for (int x = cx0; x < cx1; ++x) {
                    const gnd::Cell& c = cellAt(x, y);
                    if (pit(g, c)) continue;  // a floor's edges alone
                    const gnd::Surface* top = surfaceAt(g, c.top);
                    for (const Side& sd : kSides) {
                        const int nx = x + sd.dx, ny = y + sd.dy;
                        if (nx < 0 || ny < 0 || nx >= g.width || ny >= g.height) continue;
                        const gnd::Cell& n = cellAt(nx, ny);
                        if (!pit(g, n)) continue;
                        // the side's surface: the floor's own (right, far) or the pit's (its right, its far); the
                        // makers' picture going down - its corners 0, 1 on the holder's side, 2, 3 on the other's
                        const bool floorHolds = sd.dx > 0 || sd.dy > 0;
                        const gnd::Surface* side =
                            sd.dx > 0   ? surfaceAt(g, c.right)
                            : sd.dx < 0 ? surfaceAt(g, n.right)
                            : sd.dy > 0 ? surfaceAt(g, c.front)
                                        : surfaceAt(g, n.front);
                        // deep enough at both corners of the edge (up along -y: the pit's heights greater) - or
                        // (3.0.22-beta.47) nothing there at all: no top past it, no side picture between them
                        const bool deep = n.h[sd.pitA] - c.h[sd.floorA] >= kLeastDepth &&
                                          n.h[sd.pitB] - c.h[sd.floorB] >= kLeastDepth;
                        if (!deep && !(n.top < 0 && !side)) continue;
                        if (side && fillerOf(g, side->texture)) side = nullptr;
                        const int texture = side ? side->texture : commonest;
                        if (texture < 0) continue;
                        // the picture's upper edge (the floor's side of the makers' wall) on top, its lower below;
                        // along the edge: its corner 0 (2) at our A, 1 (3) at our B
                        float uvTop[2][2], uvLow[2][2];
                        for (int e = 0; e < 2; ++e) {
                            const int up = floorHolds ? e : 2 + e, low = floorHolds ? 2 + e : e;
                            uvTop[e][0] = side ? side->u[up] : float(e), uvTop[e][1] = side ? side->v[up] : 0.0f;
                            uvLow[e][0] = side ? side->u[low] : float(e), uvLow[e][1] = side ? side->v[low] : 1.0f;
                        }
                        // facing the floor: from the edge towards the floor cell
                        const float normal[3] = {float(-sd.dx), 0.0f, float(-sd.dy)};
                        const float slope = lightOf(sun, normal) / floorLight;
                        const uint32_t level = uint32_t(std::clamp(int(slope * 127.5f + 0.5f), 0, 255));
                        Part& part = parts[texture];
                        const uint32_t first = uint32_t(part.vertices.size());
                        for (int row = 0; row < 2; ++row)  // 0: the top's two corners, 1: the floor's
                            for (int e = 0; e < 2; ++e) {
                                const int k = e == 0 ? sd.floorA : sd.floorB;
                                terrain::Vertex v{};
                                cornerXZ(x, y, k, v.pos[0], v.pos[2]);
                                v.plain = c.h[k];  // its floor (raise's)
                                v.pos[1] = row == 0 ? c.h[k] - height : c.h[k];
                                std::copy(normal, normal + 3, v.normal);
                                v.uv[0] = row == 0 ? uvTop[e][0] : uvLow[e][0];
                                v.uv[1] = row == 0 ? uvTop[e][1] : uvLow[e][1];
                                terrain::lightmapUv(atlas, top ? int(top->lightmap) : 0, float(k & 1), float(k >> 1),
                                                    v.lm);
                                v.color = 0xFF000000u | level << 16 | level << 8 | level;
                                // the floor's corner (row by row of width + 1), none of the way off
                                v.blend = uint32_t(y + (k >> 1)) * uint32_t(g.width + 1) + uint32_t(x + (k & 1));
                                part.vertices.push_back(v);
                            }
                        part.indices.insert(part.indices.end(), {first, first + 1, first + 2, first + 1, first + 3,
                                                                 first + 2});
                    }
                }
            if (parts.empty()) continue;
            terrain::Chunk ch;
            ch.cellX = cx0;
            ch.cellY = cy0;
            ch.firstVertex = uint32_t(m.vertices.size());
            ch.firstBatch = uint32_t(m.batches.size());
            for (int k = 0; k < 3; ++k) ch.lo[k] = 1e30f, ch.hi[k] = -1e30f;
            for (const auto& [texture, part] : parts) {
                terrain::Batch b;
                b.texture = texture;
                b.layer = terrain::kGround;
                b.firstVertex = uint32_t(m.vertices.size());
                b.vertexCount = uint32_t(part.vertices.size());
                b.firstIndex = uint32_t(m.indices.size());
                b.indexCount = uint32_t(part.indices.size());
                for (const terrain::Vertex& v : part.vertices)
                    for (int k = 0; k < 3; ++k)
                        ch.lo[k] = std::min(ch.lo[k], v.pos[k]), ch.hi[k] = std::max(ch.hi[k], v.pos[k]);
                m.vertices.insert(m.vertices.end(), part.vertices.begin(), part.vertices.end());
                m.indices.insert(m.indices.end(), part.indices.begin(), part.indices.end());
                m.batches.push_back(b);
            }
            ch.vertexCount = uint32_t(m.vertices.size()) - ch.firstVertex;
            ch.batchCount = uint32_t(m.batches.size()) - ch.firstBatch;
            m.chunks.push_back(ch);
        }
    return m;
}

void raise(terrain::Mesh& m, float height) {
    for (terrain::Vertex& v : m.vertices)
        if (v.pos[1] < v.plain - 1e-3f) v.pos[1] = v.plain - height;  // a top's corner (its floor in plain)
    for (terrain::Chunk& ch : m.chunks) {
        for (int k = 0; k < 3; ++k) ch.lo[k] = 1e30f, ch.hi[k] = -1e30f;
        for (uint32_t i = ch.firstVertex; i < ch.firstVertex + ch.vertexCount && i < m.vertices.size(); ++i) {
            const float* p = m.vertices[i].pos;
            for (int k = 0; k < 3; ++k) ch.lo[k] = std::min(ch.lo[k], p[k]), ch.hi[k] = std::max(ch.hi[k], p[k]);
        }
    }
}

}  // namespace uagfx::pitwalls
