// 3.0.22-beta.22 (the owner's R-GK3ZCT, moc_pryd03 «тут тоже походу стен нету»; his «делать» 19.11): the Pyramids'
// mazes are walkways with pits between them - cells with no top, 20 under the floor, their sides the walls' pictures
// going down (moc_pryd03: 4560 of its 10000 cells). From above they read as walls; in the 3D view the camera looked
// over them. Upright faces on each walkway's edge to such a pit, up to the room's ceiling, facing the walkway
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/gnd.hpp"
#include "core/pit_walls.hpp"
#include "core/terrain.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

constexpr int kFloor = 0, kWall = 1, kStone = 2;

int surfaceOf(gnd::Ground& g, int texture) {
    gnd::Surface s;
    const float u[4] = {0, 1, 0, 1}, v[4] = {0, 0, 1, 1};
    std::copy(u, u + 4, s.u);
    std::copy(v, v + 4, s.v);
    s.texture = int16_t(texture);
    g.surfaces.push_back(s);
    return int(g.surfaces.size() - 1);
}

gnd::Cell& cellAt(gnd::Ground& g, int x, int y) { return g.cells[size_t(y) * size_t(g.width) + size_t(x)]; }

// 6 x 3: a walkway along y 0 and another along y 2 (their floor at 0), pits along y 1 - no top, 20 under them (up along
// -y: +20) - but at x 5 only 4 under (a step, no wall). The first walkway's far sides towards the pit carry the walls'
// picture on x 0-2 (as the makers laid them, going down), x 3-4 none (the map's commonest then); the second's near
// sides are the pits' own far sides (the pit holds them) on x 0-4
gnd::Ground maze() {
    gnd::Ground g;
    g.width = 6, g.height = 3;
    g.zoom = 10.0f;
    g.textures = {"floor.bmp", "wall.bmp", "stone.bmp"};
    g.lightmaps.resize(1);
    g.cells.resize(18);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 6; ++x) {
            gnd::Cell& c = cellAt(g, x, y);
            const bool pit = y == 1;
            for (float& corner : c.h) corner = pit ? (x == 5 ? 4.0f : 20.0f) : 0.0f;
            c.top = pit ? -1 : surfaceOf(g, kFloor);
        }
    for (int x = 0; x <= 2; ++x) cellAt(g, x, 0).front = surfaceOf(g, kWall);
    for (int x = 0; x <= 4; ++x) cellAt(g, x, 1).front = surfaceOf(g, kWall);
    cellAt(g, 5, 0).front = surfaceOf(g, kStone);  // the step's side: a picture of its own, no wall over it
    cellAt(g, 5, 1).front = surfaceOf(g, kStone);  // .47: and on its far side (nothing there would be a room's void)
    return g;
}

size_t quadsOf(const terrain::Mesh& m) {
    size_t n = 0;
    for (const terrain::Batch& b : m.batches) n += b.indexCount / 6;
    return n;
}

}  // namespace

TEST(pit_walls_stand_on_the_walkways_edges) {
    const gnd::Ground g = maze();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Sun sun;  // the game's own when a map has none
    const terrain::Mesh m = pitwalls::build(g, atlas, sun, 30.0f);
    // the first walkway's far edge on x 0-4 and the second's near edge on x 0-4: ten walls; none over the step at x 5
    CHECK_EQ(quadsOf(m), size_t(10));
    CHECK(!m.chunks.empty());
    int toFirst = 0, toSecond = 0;
    for (const terrain::Vertex& v : m.vertices) {
        CHECK(std::fabs(v.pos[1]) < 1e-4f || std::fabs(v.pos[1] + 30.0f) < 1e-4f);  // its floor, 30 over it
        CHECK(std::fabs(v.normal[1]) < 1e-4f && terrain::isWall(v));                 // upright
        CHECK(std::fabs(v.pos[2] - (-5.0f)) < 1e-4f || std::fabs(v.pos[2] - 5.0f) < 1e-4f);  // on the pits' edges
        // facing its walkway: the first lies at -z of the pits, the second at +z
        toFirst += v.pos[2] < 0.0f && v.normal[2] < -0.99f ? 1 : 0;
        toSecond += v.pos[2] > 0.0f && v.normal[2] > 0.99f ? 1 : 0;
        CHECK(v.pos[0] < 5.0f * 10.0f - 30.0f + 1e-3f);  // x 0-4: none over the step (x 5: 20..30)
    }
    CHECK_EQ(toFirst, 20);
    CHECK_EQ(toSecond, 20);
    // the walls' pictures: the pits' own sides where the makers laid them, the map's commonest where not - no floor
    bool wall = false, other = false;
    for (const terrain::Batch& b : m.batches) (b.texture == kWall ? wall : other) = true;
    CHECK(wall && !other);
    // their tops raised to the room's ceiling once the models tell it
    terrain::Mesh r = m;
    pitwalls::raise(r, 22.0f);
    for (const terrain::Vertex& v : r.vertices)
        CHECK(std::fabs(v.pos[1]) < 1e-4f || std::fabs(v.pos[1] + 22.0f) < 1e-4f);
    for (const terrain::Chunk& c : r.chunks) CHECK(c.lo[1] > -22.0f - 1e-3f);
}

TEST(pit_walls_none_on_open_or_stepped_ground) {
    gnd::Ground g = maze();
    for (int x = 0; x < 6; ++x) cellAt(g, x, 1).top = surfaceOf(g, kStone);  // the pits floored: a lower room
    CHECK(pitwalls::build(g, terrain::lightmapAtlas(g), terrain::Sun{}, 30.0f).vertices.empty());
    gnd::Ground flat;
    flat.width = flat.height = 2;
    flat.cells.resize(4);
    for (gnd::Cell& c : flat.cells) {
        for (float& corner : c.h) corner = 0.0f;
        c.top = surfaceOf(flat, 0);
    }
    flat.textures = {"floor.bmp"};
    CHECK(pitwalls::build(flat, terrain::lightmapAtlas(flat), terrain::Sun{}, 30.0f).vertices.empty());
    CHECK(pitwalls::build(gnd::Ground{}, terrain::Atlas{}, terrain::Sun{}, 30.0f).vertices.empty());
}

// 3.0.22-beta.47 (the owner: «да, для всех»): every map not under the sky - a house, a hall, a tower, a dungeon, the
// novice school's rooms (moc_pryd03 alone before); none under the sky
TEST(pit_walls_on_every_map_under_a_roof) {
    for (const char* m : {"moc_pryd03", "moc_pryd04", "c_tower3", "prt_in", "geffen_in", "in_sphinx1", "new_1-1",
                          "new_1-4", "new_zone02", "pay_dun00"})
        CHECK(pitwalls::wallsFor(m));
    for (const char* m : {"prontera", "prt_fild08", "new_zone01", "new_1-2", ""}) CHECK(!pitwalls::wallsFor(m));
}

// 3.0.22-beta.47 (the owner 21.11: the novice school's rooms in 3D - «стенки прозрачные и видны другие комнаты», «да,
// для всех»): a house's room floats in the void, its sides open for the camera from above - a floor's edge to a cell
// of nothing at all (no top, no side picture between them) at its own height gets a wall too; a step (a side picture
// there) still none
TEST(pit_walls_close_a_room_floating_in_the_void) {
    gnd::Ground g;
    g.width = 3, g.height = 1;
    g.textures = {"floor.bmp", "wall.bmp", "stone.bmp"};
    g.lightmaps.resize(1);
    g.cells.resize(3);
    for (int x = 0; x < 3; ++x) {
        gnd::Cell& c = cellAt(g, x, 0);
        for (float& corner : c.h) corner = 0.0f;
        c.top = x < 2 ? surfaceOf(g, kFloor) : -1;  // two floor cells, then nothing
    }
    cellAt(g, 0, 0).front = surfaceOf(g, kWall);  // a side picture somewhere: the commonest
    const terrain::Mesh m = pitwalls::build(g, terrain::lightmapAtlas(g), terrain::Sun{}, 40.0f);
    CHECK_EQ(quadsOf(m), size_t(1));  // the second floor's right side, to the void
    cellAt(g, 1, 0).right = surfaceOf(g, kStone);  // a side between them: the makers' step, no wall
    CHECK_EQ(quadsOf(pitwalls::build(g, terrain::lightmapAtlas(g), terrain::Sun{}, 40.0f)), size_t(0));
}
