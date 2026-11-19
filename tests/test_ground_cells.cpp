#include <cmath>
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

// 3.0.20-beta.46 (R-HGTAKK «в пронте мигает трава, но по идее ее на асфальте быть не должно»): the 3D view's grass
// asked the picture's pixel at each root whether the ground is green - lit and graded as it is shown, a grey paving
// stone's joint passes one frame and not the next. Where the map's own ground is known a cell grows grass by its own
// texture: the part of the picture the cell shows, a quarter of it green at least (the game's own picture, unlit - no
// time of day in it); a cell of paving has none
TEST(ground_cells_grass_by_the_cells_own_texture) {
    gnd::Ground g;
    g.width = 6, g.height = 1;
    g.zoom = 10.0f;
    g.textures = {"grass.bmp", "paving.bmp", "sheet.bmp", "missing.bmp", "olive.bmp"};
    // the pictures 8 x 8: grass green; paving grey with three mossy texels; a sheet - its left half grass, its right
    // half paving (two tiles side by side); the fourth not found
    // an olive meadow - green by 0.4 of the grass's measure all over: the grass grows thinner there, but it grows
    std::vector<uint32_t> grass(64, 0xFF4C8A32u), paving(64, 0xFF8C8A84u), sheet(64), olive(64, 0xFFC8C496u);
    paving[5] = paving[20] = paving[41] = 0xFF5A8C3Cu;
    for (int i = 0; i < 64; ++i) sheet[size_t(i)] = i % 8 < 4 ? 0xFF4C8A32u : 0xFF8C8A84u;
    const std::vector<ground_cells::Picture> pictures = {
        {8, 8, grass.data()}, {8, 8, paving.data()}, {8, 8, sheet.data()}, {0, 0, nullptr}, {8, 8, olive.data()}};
    const auto surface = [&](int texture, float u0, float u1) {
        gnd::Surface s;
        s.u[0] = u0, s.u[1] = u1, s.u[2] = u0, s.u[3] = u1;
        s.v[0] = 0.0f, s.v[1] = 0.0f, s.v[2] = 1.0f, s.v[3] = 1.0f;
        s.texture = int16_t(texture);
        g.surfaces.push_back(s);
        return int32_t(g.surfaces.size() - 1);
    };
    const int32_t tops[6] = {surface(0, 0.0f, 1.0f), surface(1, 0.0f, 1.0f), surface(2, 0.0f, 0.5f),
                             surface(2, 0.5f, 1.0f), surface(3, 0.0f, 1.0f), surface(4, 0.0f, 1.0f)};
    for (int x = 0; x < 6; ++x) {
        gnd::Cell c{};
        c.top = tops[x];
        g.cells.push_back(c);
    }
    const std::vector<uint8_t> grassy = ground_cells::grassyCells(g, pictures);
    // grass; paving (its moss too little); the sheet's grass tile; its paving tile; a picture not found - as before;
    // the olive meadow
    CHECK(grassy == (std::vector<uint8_t>{1, 0, 1, 0, 1, 1}));
    CHECK(ground_cells::greenOfCell(g, 0, pictures) > 0.99f && ground_cells::greenOfCell(g, 4, pictures) < 0.0f);
    CHECK(std::fabs(ground_cells::greenOfCell(g, 5, pictures) - 0.4f) < 0.05f);
    CHECK(ground_cells::greenOfCell(g, 1, pictures) < 0.06f);  // three mossy texels of 64
    g.cells[0].top = -1;  // no top: the land past the ground's edge may lie there - not this rule's to say
    CHECK(ground_cells::grassyCells(g, pictures)[0] == 1);
}

// 3.0.20-beta.53 (the owner's «делай 1-9», 8 - «трава не на камне у всех»): beta.46's rule needed the own ground - its
// pictures all in memory at once. Without it (the own ground off, no room for it, Direct3D 11) the same answer with
// one picture in memory at a time: each texture a top wears asked for once, its cells judged, the next
TEST(ground_cells_grass_by_the_cells_own_texture_a_picture_at_a_time) {
    gnd::Ground g;
    g.width = 5, g.height = 1;
    g.zoom = 10.0f;
    g.textures = {"grass.bmp", "paving.bmp", "unused.bmp", "missing.bmp", "wall.bmp"};
    std::vector<uint32_t> grass(64, 0xFF4C8A32u), paving(64, 0xFF8C8A84u);
    const auto surface = [&](int texture) {
        gnd::Surface s;
        s.u[0] = 0.0f, s.u[1] = 1.0f, s.u[2] = 0.0f, s.u[3] = 1.0f;
        s.v[0] = 0.0f, s.v[1] = 0.0f, s.v[2] = 1.0f, s.v[3] = 1.0f;
        s.texture = int16_t(texture);
        g.surfaces.push_back(s);
        return int32_t(g.surfaces.size() - 1);
    };
    const int32_t tops[5] = {surface(1), surface(0), surface(3), surface(1), surface(0)};
    for (int x = 0; x < 5; ++x) {
        gnd::Cell c{};
        c.top = tops[x];
        g.cells.push_back(c);
    }
    g.cells[2].front = surface(4);  // a wall's texture alone: no top wears it - never asked for
    std::vector<size_t> asked;
    std::vector<uint32_t> held;  // the one picture in memory
    const auto pictureOf = [&](size_t texture, ground_cells::Picture& out) {
        asked.push_back(texture);
        if (texture > 1) return false;  // not found
        held = texture == 0 ? grass : paving;
        out = {8, 8, held.data()};
        return true;
    };
    const std::vector<uint8_t> grassy = ground_cells::grassyCells(g, pictureOf);
    // paving, grass, a picture not found (as before: the screen's colour decides), paving, grass
    CHECK(grassy == (std::vector<uint8_t>{0, 1, 1, 0, 1}));
    // each texture the tops wear once, in the textures' order; the unused one and the wall's never
    CHECK(asked == (std::vector<size_t>{0, 1, 3}));
    // the same as with them all at once
    const std::vector<ground_cells::Picture> pictures = {
        {8, 8, grass.data()}, {8, 8, paving.data()}, {0, 0, nullptr}, {0, 0, nullptr}, {0, 0, nullptr}};
    CHECK(grassy == ground_cells::grassyCells(g, pictures));
    g.cells[1].top = -1;  // no top: not this rule's to say
    CHECK(ground_cells::grassyCells(g, pictureOf)[1] == 1);
    CHECK(ground_cells::grassyCells(gnd::Ground{}, pictureOf).empty());
}

// 3.0.22-beta.19 (the owner's R-9J8RH5, hugel «трава на брусчатке и немножко мигает»): Hugel's cobbles - pale stones,
// green moss in their joints, green by 0.17 of the grass's measure, over kGrassy (Prontera's paving 0.10 at most) -
// grow no grass: a cell mostly pale stone (kPaving) is paving, grass on it only where it is as green as a lawn (a
// border's half); a forest floor as green, dark soil under its blades, grows it as before; the olive meadow too
TEST(ground_cells_no_grass_on_mossy_cobbles) {
    gnd::Ground g;
    g.width = 4, g.height = 1;
    g.zoom = 10.0f;
    g.textures = {"cobbles.bmp", "forest.bmp", "border.bmp", "olive.bmp"};
    std::vector<uint32_t> cobbles(64, 0xFFC8C8C3u), forest(64, 0xFF46372Au), border(64), olive(64, 0xFFC8C496u);
    for (int i = 0; i < 64; ++i) {
        if (i % 6 == 0) cobbles[size_t(i)] = 0xFF3C8C32u;  // the moss in the joints, a sixth
        if (i % 4 == 0) forest[size_t(i)] = 0xFF4C8A32u;   // blades over dark soil, a quarter
        border[size_t(i)] = i % 8 < 4 ? 0xFF4C8A32u : 0xFFC8C8C3u;  // a lawn's edge on the paving
    }
    const std::vector<ground_cells::Picture> pictures = {
        {8, 8, cobbles.data()}, {8, 8, forest.data()}, {8, 8, border.data()}, {8, 8, olive.data()}};
    for (int x = 0; x < 4; ++x) {
        gnd::Surface s;
        s.u[0] = 0.0f, s.u[1] = 1.0f, s.u[2] = 0.0f, s.u[3] = 1.0f;
        s.v[0] = 0.0f, s.v[1] = 0.0f, s.v[2] = 1.0f, s.v[3] = 1.0f;
        s.texture = int16_t(x);
        g.surfaces.push_back(s);
        gnd::Cell c{};
        c.top = x;
        g.cells.push_back(c);
    }
    const std::vector<uint8_t> grassy = ground_cells::grassyCells(g, pictures);
    CHECK(grassy == (std::vector<uint8_t>{0, 1, 1, 1}));
    const ground_cells::CellLook cobble = ground_cells::lookOfCell(g, 0, pictures);
    CHECK(cobble.green >= ground_cells::kGrassy && cobble.stone >= ground_cells::kPaving);  // the old rule's grass
    CHECK(ground_cells::lookOfCell(g, 1, pictures).stone < 0.05f);
    CHECK(ground_cells::lookOfCell(g, 3, pictures).stone < 0.05f);  // olive: too warm for grey stone
    const ground_cells::PictureOf pictureOf = [&](size_t t, ground_cells::Picture& out) {
        if (t >= pictures.size()) return false;
        out = pictures[t];
        return true;
    };
    CHECK(ground_cells::grassyCells(g, pictureOf) == grassy);
}
