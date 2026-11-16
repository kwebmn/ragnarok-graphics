#include <utility>
#include <vector>

#include "core/ground_cells.hpp"
#include "core/terrain.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A cell of `side` at (ix, iz), its corners at height y (up along -y).
void cell(std::vector<seams::Triangle>& out, int ix, int iz, float side, float y) {
    seams::Vertex c[4];
    for (int k = 0; k < 4; ++k) {
        c[k].pos[0] = (float(ix) + float(k & 1)) * side + 3.0f;
        c[k].pos[1] = y;
        c[k].pos[2] = (float(iz) + float(k >> 1)) * side - 7.0f;
    }
    seams::Triangle a, b;
    a.v[0] = c[0], a.v[1] = c[1], a.v[2] = c[2];
    b.v[0] = c[2], b.v[1] = c[1], b.v[2] = c[3];
    out.push_back(a);
    out.push_back(b);
}

}  // namespace

// 3.0.18-beta.252 (R-9UNG8S): the halls' floors of a castle (a carpet, green tiles - green as grass) taken out of the
// grass's ground - a cell whose middle the caller says is such a floor has no ground then; counted
TEST(ground_cells_the_halls_floors_grow_no_grass) {
    ground_cells::Grid g;
    g.originX = 100.0f;
    g.originZ = -50.0f;
    g.side = 10.0f;
    g.width = 3;
    g.height = 2;
    g.span = {-1.0f, 2.0f, -1.0f, 2.0f, -1.0f, 2.0f, 1e30f, -1e30f, -1.0f, 2.0f, -1.0f, 2.0f};
    std::vector<std::pair<float, float>> asked;
    const size_t dropped = ground_cells::dropWhere(g, [&](float x, float z) {
        asked.emplace_back(x, z);
        return x > 120.0f || (x < 110.0f && z > -40.0f);  // the third column; the first cell of the second row
    });
    CHECK_EQ(dropped, size_t(2));            // (2, 0) and (2, 1); (0, 1) had no ground already
    CHECK_EQ(asked.size(), size_t(5));        // the cells with ground, at their middles
    CHECK(asked[0].first == 105.0f && asked[0].second == -45.0f);
    CHECK(g.span[4] > g.span[5] && g.span[10] > g.span[11]);  // the third column: no ground
    CHECK(g.span[0] <= g.span[1] && g.span[2] <= g.span[3] && g.span[8] <= g.span[9]);
    CHECK(g.span[6] > g.span[7]);
    ground_cells::Grid none;
    CHECK_EQ(ground_cells::dropWhere(none, [](float, float) { return true; }), size_t(0));
}

namespace {

// a triangle of a mesh's ground (its own batch): corners x, y, z; the plain ground's height each `plain`
void meshTriangle(terrain::Mesh& m, const float a[3], const float b[3], const float c[3], float plain,
                  uint8_t layer = terrain::kGround, bool wall = false) {
    terrain::Batch batch;
    batch.layer = layer;
    batch.firstVertex = uint32_t(m.vertices.size());
    batch.vertexCount = 3;
    batch.firstIndex = uint32_t(m.indices.size());
    batch.indexCount = 3;
    for (const float* p : {a, b, c}) {
        terrain::Vertex v{};
        v.pos[0] = p[0], v.pos[1] = p[1], v.pos[2] = p[2];
        v.plain = plain;
        v.normal[1] = wall ? 0.0f : -1.0f;  // terrain::isWall: a wall's normal lies across
        m.vertices.push_back(v);
    }
    for (uint32_t i = 0; i < 3; ++i) m.indices.push_back(i);
    m.batches.push_back(batch);
}

}  // namespace

// 3.0.20-beta.41 (the audit of the 3D view's grass): over the own ground in the 3D view none of the game's ground is
// noted, and the grass's cells stayed those of the frames before it - a part of the map, or another map's. The whole
// map's cells from the own ground's meshes: a cell's heights those its ground's triangles span (the smooth ground and
// the plain one it eases onto; the walls left out), none where it has no ground; the world's places by the map's axes
TEST(ground_cells_of_the_own_grounds_meshes_the_whole_map) {
    terrain::Mesh smooth;  // 3 x 1 cells of 10: file x -15..15, z -5..5
    const float a[3] = {-15.0f, -5.5f, -5.0f}, b[3] = {-5.0f, -5.0f, -5.0f}, c[3] = {-15.0f, -5.0f, 5.0f};
    const float d[3] = {-5.0f, -4.0f, 5.0f};
    meshTriangle(smooth, a, b, c, -5.0f);  // cell 0: its smooth ground from -5.5 ...
    meshTriangle(smooth, c, b, d, -5.0f);  // ... to -4
    const float w0[3] = {-5.0f, -5.0f, -5.0f}, w1[3] = {-5.0f, -5.0f, 5.0f}, w2[3] = {-5.0f, -40.0f, 0.0f};
    meshTriangle(smooth, w0, w1, w2, -5.0f);  // a wall at cell 0's side: upright, not the ground's
    const float n0[3] = {-12.0f, -90.0f, 0.0f}, n1[3] = {-12.0f, -90.0f, 0.0f}, n2[3] = {-12.0f, 60.0f, 0.0f};
    meshTriangle(smooth, n0, n1, n2, -5.0f, terrain::kGround, true);  // a wall's face of no size: none of the ground's
    const float e[3] = {5.0f, 2.0f, -5.0f}, f[3] = {15.0f, 2.0f, -5.0f}, g2[3] = {5.0f, 2.0f, 5.0f};
    meshTriangle(smooth, e, f, g2, 3.0f);  // cell 2: at 2, the plain ground under it at 3; cell 1: none
    meshTriangle(smooth, e, f, g2, 3.0f, terrain::kNeighbour);  // a strip between textures: not counted again
    const ground_cells::Grid g = ground_cells::ofMeshes({&smooth}, 3, 1, 10.0f, 1, 1, 1);
    CHECK(g.width == 3 && g.height == 1 && g.side == 10.0f && g.originX == -15.0f && g.originZ == -5.0f);
    CHECK(g.span.size() == 6);
    CHECK(g.span[0] == -5.5f && g.span[1] == -4.0f);  // the wall's -40 left out
    CHECK(g.span[2] > g.span[3]);                     // no ground
    CHECK(g.span[4] == 2.0f && g.span[5] == 3.0f);
    // the map's x the other way round, its heights too (up along +y): the cells mirrored, the spans turned over
    const ground_cells::Grid m = ground_cells::ofMeshes({&smooth}, 3, 1, 10.0f, -1, -1, 1);
    CHECK(m.span[0] == -3.0f && m.span[1] == -2.0f && m.span[2] > m.span[3]);
    CHECK(m.span[4] == 4.0f && m.span[5] == 5.5f);
    CHECK(ground_cells::ofMeshes({}, 3, 1, 10.0f, 1, 1, 1).side == 0.0f);  // nothing to make it of
}
