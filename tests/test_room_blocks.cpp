// 3.0.20-beta.71 (the owner's pick of the interiors' options, «ок, а»: the black blocks at a room's edge given the
// room's own wall picture - check list 18, «есть стены черные, они там не продуманы»): in a house's map the makers
// raised blocks of their black filler beside the rooms; the side towards the room carries the wall's picture, the
// top and the other sides are black - from above nobody saw them, from the eyes' level they stand in the view
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/gnd.hpp"
#include "core/room_blocks.hpp"
#include "core/rooms.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

constexpr int kFloor = 0, kWall = 1, kBlack = 2;

struct House {
    gnd::Ground g;
    int wallLight = 0;
};

int surfaceOf(gnd::Ground& g, int texture, uint16_t lightmap = 0) {
    gnd::Surface s;
    const float u[4] = {0, 1, 0, 1}, v[4] = {0, 0, 1, 1};
    std::copy(u, u + 4, s.u);
    std::copy(v, v + 4, s.v);
    s.texture = int16_t(texture);
    s.lightmap = lightmap;
    g.surfaces.push_back(s);
    return int(g.surfaces.size() - 1);
}

gnd::Cell& cellAt(gnd::Ground& g, int x, int y) { return g.cells[size_t(y) * size_t(g.width) + size_t(x)]; }

// 12 x 12: a room of 6 x 6 at 2..7; a row of blocks 30 up along its +x side (x 8); filler at the floor's level past
// them; the room's far side (y 8 on) open - filler 20 lower; a block far off in the corner
House house() {
    House h;
    gnd::Ground& g = h.g;
    g.width = g.height = 12;
    g.zoom = 10.0f;
    g.textures = {"floor.bmp", "wall.bmp", "black.bmp"};
    g.lightmaps.resize(3);
    std::fill(g.lightmaps[2].brightness, g.lightmaps[2].brightness + 64, uint8_t(200));  // the wall's: lit
    std::fill(g.lightmaps[2].rgb, g.lightmaps[2].rgb + 192, uint8_t(30));
    g.cells.resize(144);
    h.wallLight = 2;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 12; ++x) {
            gnd::Cell& c = cellAt(g, x, y);
            const bool room = x >= 2 && x <= 7 && y >= 2 && y <= 7;
            const bool block = (x == 8 && y >= 2 && y <= 7) || (x == 11 && y == 11);
            const float up = block ? 30.0f : (!room && y >= 8 && x < 11) ? -20.0f : 0.0f;
            for (float& corner : c.h) corner = -up;  // up along -y
            c.top = surfaceOf(g, room ? kFloor : kBlack, room ? 1 : 0);
        }
    // the room's wall towards the blocks: its own picture on five of six cells, the sixth left black by the makers
    for (int y = 2; y <= 7; ++y) cellAt(g, 7, y).right = surfaceOf(g, y == 7 ? kBlack : kWall, uint16_t(h.wallLight));
    return h;
}

// the picture a surface shows (the dressed faces lie on copies of the wall's texture: the same name, another number)
std::string pictureOf(const gnd::Ground& g, const gnd::Surface& s) { return g.textures[size_t(s.texture)]; }

bool within01(const gnd::Surface& s) {
    for (int k = 0; k < 4; ++k)
        if (!(s.u[k] >= 0.0f && s.u[k] <= 1.0f && s.v[k] >= 0.0f && s.v[k] <= 1.0f)) return false;
    return true;
}

}  // namespace

TEST(room_blocks_get_their_rooms_wall_picture) {
    House h = house();
    gnd::Ground& g = h.g;
    const rooms::Map rooms = rooms::build(g);
    CHECK_EQ(rooms.count, 1);
    const int wallBefore = cellAt(g, 7, 3).right;
    const roomblocks::Done done = roomblocks::dress(g, rooms);
    CHECK_EQ(done.rooms, size_t(1));
    CHECK_EQ(done.tops, size_t(6));
    // a block's top: the wall's picture's upper band (its rim: a wall with its windows laid flat is no wall's top),
    // run over the cells across; lit by the walls' light evened out - a lightmap of its own, the wall's values in it
    CHECK_EQ(g.lightmaps.size(), size_t(4));
    for (int y = 2; y <= 7; ++y) {
        const gnd::Surface& top = g.surfaces[size_t(cellAt(g, 8, y).top)];
        CHECK_EQ(pictureOf(g, top), std::string("wall.bmp"));
        CHECK(int(top.texture) != kWall);  // a copy of its own: the view from above shows these faces otherwise
        CHECK_EQ(done.fromAbove[size_t(top.texture)], kBlack);  // there: the makers' black, as it was
        CHECK_EQ(int(top.lightmap), 3);
        CHECK(within01(top));
        CHECK(std::fabs(top.u[1] - top.u[0]) > 0.1f);  // across: laid out over the cell
        for (int k = 0; k < 4; ++k) CHECK(top.v[k] <= roomblocks::kCapShare + 1e-6f);
        CHECK(std::fabs(top.v[2] - top.v[0]) > 0.01f);  // the band, not one row
    }
    CHECK_EQ(int(g.lightmaps[3].brightness[27]), 200);
    // no colour of its own added: from above these faces show the makers' black, and black must stay black
    CHECK_EQ(int(g.lightmaps[3].rgb[100]), 0);
    // its side away from the room had no face at all: one made, the picture's top at the block's edge
    for (int y = 2; y <= 7; ++y) {
        const int face = cellAt(g, 8, y).right;
        CHECK(face >= 0);
        if (face < 0) continue;
        const gnd::Surface& s = g.surfaces[size_t(face)];
        CHECK_EQ(pictureOf(g, s), std::string("wall.bmp"));
        CHECK_EQ(done.fromAbove[size_t(s.texture)], roomblocks::kNotDrawn);  // from above: no face, as the makers left
        CHECK(within01(s));
        CHECK_EQ(s.v[0], 0.0f);  // the owner's edge is the block's: the top
        CHECK_EQ(s.v[1], 0.0f);
        CHECK(s.v[2] > 0.5f && s.v[3] > 0.5f);  // thirty down: the whole picture, as the room's wall is thirty tall
    }
    // the room's own wall faces stay the makers'; the one they left black is dressed, its top at the block's edge too
    CHECK_EQ(cellAt(g, 7, 3).right, wallBefore);
    const gnd::Surface& was = g.surfaces[size_t(cellAt(g, 7, 7).right)];
    CHECK_EQ(pictureOf(g, was), std::string("wall.bmp"));
    CHECK_EQ(done.fromAbove[size_t(was.texture)], kBlack);
    CHECK(was.v[2] == 0.0f && was.v[3] == 0.0f && was.v[0] > 0.5f);  // the block is the neighbour there
    // between two blocks of one height: no face; the end blocks' ends: faces down to the filler beside them
    CHECK_EQ(cellAt(g, 8, 3).front, -1);
    CHECK(cellAt(g, 8, 7).front >= 0);  // towards the open side, 20 lower
    CHECK(cellAt(g, 8, 1).front >= 0);  // the cell before the first block owns that side
    if (cellAt(g, 8, 1).front >= 0) {
        const gnd::Surface& end = g.surfaces[size_t(cellAt(g, 8, 1).front)];
        CHECK_EQ(pictureOf(g, end), std::string("wall.bmp"));
        CHECK(end.v[2] == 0.0f && end.v[0] > 0.5f);
    }
    CHECK_EQ(done.sides, size_t(1));     // the one the makers left black
    CHECK_EQ(done.made, size_t(6 + 2));  // the six away from the room, the two ends
    // the ground's own textures as they are from above too
    CHECK_EQ(done.fromAbove.size(), g.textures.size());
    CHECK_EQ(done.fromAbove[kFloor], roomblocks::kAsItIs);
    CHECK_EQ(done.fromAbove[kWall], roomblocks::kAsItIs);
    CHECK_EQ(done.fromAbove[kBlack], roomblocks::kAsItIs);
}

TEST(room_blocks_leave_the_rest_as_it_is) {
    House h = house();
    gnd::Ground& g = h.g;
    const rooms::Map rooms = rooms::build(g);
    const size_t surfacesBefore = g.surfaces.size();
    const int floorTop = cellAt(g, 4, 4).top, openTop = cellAt(g, 4, 9).top, levelTop = cellAt(g, 9, 4).top,
              farTop = cellAt(g, 11, 11).top;
    roomblocks::dress(g, rooms);
    CHECK(g.surfaces.size() > surfacesBefore);
    CHECK_EQ(cellAt(g, 4, 4).top, floorTop);    // the room's floor
    CHECK_EQ(cellAt(g, 4, 9).top, openTop);     // the open side: the filler lying lower stays (not asked for)
    CHECK_EQ(cellAt(g, 9, 4).top, levelTop);    // filler at the floor's level past the blocks
    CHECK(roomblocks::kReach < 4);
    CHECK_EQ(cellAt(g, 11, 11).top, farTop);    // a block four cells from the room: past kReach
    CHECK_EQ(int(g.surfaces[size_t(farTop)].texture), kBlack);
    CHECK_EQ(cellAt(g, 4, 7).front, -1);        // no wall built on the open side
    // done once: a second go finds nothing black to dress
    const roomblocks::Done again = roomblocks::dress(g, rooms);
    CHECK_EQ(again.tops, size_t(0));
    CHECK_EQ(again.sides, size_t(0));
    CHECK_EQ(again.made, size_t(0));
}

TEST(room_blocks_need_a_wall_picture_to_copy) {
    // a room whose walls carry no picture at all (their walls are models: prt_in) - nothing to lay on the blocks
    House h = house();
    gnd::Ground& g = h.g;
    for (int y = 2; y <= 7; ++y) cellAt(g, 7, y).right = -1;
    const rooms::Map rooms = rooms::build(g);
    const roomblocks::Done done = roomblocks::dress(g, rooms);
    CHECK_EQ(done.tops, size_t(0));
    CHECK_EQ(done.sides, size_t(0));
    CHECK_EQ(done.made, size_t(0));
    CHECK_EQ(done.rooms, size_t(0));
    CHECK_EQ(g.textures.size(), size_t(3));  // no copies made
    // no rooms at all, an empty ground
    gnd::Ground none;
    CHECK_EQ(roomblocks::dress(none, rooms::Map{}).tops, size_t(0));
}

// A wide picture laid over several cells (the makers' own way: a 4-cell wall) goes on over the blocks the same way
TEST(room_blocks_keep_the_pictures_run_over_the_cells) {
    House h = house();
    gnd::Ground& g = h.g;
    for (int y = 2; y <= 6; ++y) {  // each wall face a quarter of the picture across
        gnd::Surface& s = g.surfaces[size_t(cellAt(g, 7, y).right)];
        const float from = float(y % 4) * 0.25f;
        const float u[4] = {from + 0.25f, from, from + 0.25f, from};
        std::copy(u, u + 4, s.u);
    }
    const rooms::Map rooms = rooms::build(g);
    roomblocks::dress(g, rooms);
    for (int y = 2; y <= 7; ++y) {
        const gnd::Surface& side = g.surfaces[size_t(cellAt(g, 8, y).right)];
        CHECK(std::fabs(std::fabs(side.u[1] - side.u[0]) - 0.25f) < 1e-4f);  // a quarter a cell
        CHECK(within01(side));
        const gnd::Surface& top = g.surfaces[size_t(cellAt(g, 8, y).top)];
        CHECK(std::fabs(std::fabs(top.u[1] - top.u[0]) - 0.25f) < 1e-4f);
    }
    // neighbours meet: a cell's far edge is the next one's near edge (but where the picture starts again)
    const gnd::Surface& a = g.surfaces[size_t(cellAt(g, 8, 4).right)];
    const gnd::Surface& b = g.surfaces[size_t(cellAt(g, 8, 5).right)];
    CHECK(std::fabs(a.u[0] - b.u[1]) < 1e-4f);
}

// The picture is the WALL's: at a room's edges the makers' faces are also steps and platforms' rims a few units tall,
// in the floor's tiles (payon_in01: fay_tile004 on more faces than any wall) - only the faces that stand against a
// block count, by their height
TEST(room_blocks_take_the_walls_picture_not_a_steps) {
    House h = house();
    gnd::Ground& g = h.g;
    g.textures.push_back("tile.bmp");
    const int kTile = 3;
    // a step two units down along the room's whole -x side and its -y side: twelve faces in the floor's tile
    for (int y = 2; y <= 7; ++y) {
        for (float& corner : cellAt(g, 1, y).h) corner = 2.0f;
        cellAt(g, 1, y).right = surfaceOf(g, kTile, 1);
    }
    for (int x = 2; x <= 7; ++x) {
        for (float& corner : cellAt(g, x, 1).h) corner = 2.0f;
        cellAt(g, x, 1).front = surfaceOf(g, kTile, 1);
    }
    const rooms::Map rooms = rooms::build(g);
    const roomblocks::Done done = roomblocks::dress(g, rooms);
    CHECK_EQ(done.tops, size_t(6));
    for (int y = 2; y <= 7; ++y)
        CHECK_EQ(pictureOf(g, g.surfaces[size_t(cellAt(g, 8, y).top)]), std::string("wall.bmp"));
}
