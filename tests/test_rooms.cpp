// 3.0.20-beta.57 (R-J3GKD9, the owner in payon_in01's 3D view: «тут другие комнаты видны»): a house's map keeps its
// rooms side by side - payon_in01 twelve of them, prt_in twenty-eight - each its own patch of floor with no ground
// (or the makers' black filler) between; from above one room fills the screen, from the eyes' level the others stand
// lit in the dark behind the room's open sides. The rooms of a map: its cells with a top of no filler texture, joined
// side to side (/root/uagfx-tools/audit/ceiling/roomcheck.cpp) - for the renderer to draw the one the player is in
#include <cmath>

#include "core/cave_vault.hpp"
#include "core/config.hpp"
#include "core/gnd.hpp"
#include "core/live_config.hpp"
#include "core/rooms.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// a map 30 x 20: a room of 6 x 5 cells from (2, 2) with its floor 10 up, another of 8 x 6 from (18, 10) 4 up, a
// closet of 2 x 2 at (12, 2), black filler from (2, 12) for 5 x 5, no ground elsewhere
gnd::Ground house() {
    gnd::Ground g;
    g.width = 30, g.height = 20;
    g.zoom = 10.0f;
    g.textures = {"floor.bmp", "BLACK.BMP"};
    for (int t = 0; t < 2; ++t) {
        gnd::Surface s;
        s.texture = int16_t(t);
        g.surfaces.push_back(s);
    }
    g.cells.assign(size_t(g.width) * size_t(g.height), gnd::Cell{});
    const auto lay = [&](int x0, int y0, int w, int h, float up, int surface) {
        for (int y = y0; y < y0 + h; ++y)
            for (int x = x0; x < x0 + w; ++x) {
                gnd::Cell& c = g.cells[size_t(y) * size_t(g.width) + size_t(x)];
                for (float& corner : c.h) corner = -up;
                c.top = surface;
            }
    };
    lay(2, 2, 6, 5, 10.0f, 0);
    lay(18, 10, 8, 6, 4.0f, 0);
    lay(12, 2, 2, 2, 7.0f, 0);
    lay(2, 12, 5, 5, 0.0f, 1);
    return g;
}

uint8_t at(const std::vector<uint8_t>& cells, int x, int y) { return cells[size_t(y) * 30 + size_t(x)]; }

}  // namespace

TEST(rooms_are_the_floors_joined_side_to_side) {
    const rooms::Map m = rooms::build(house());
    CHECK(m.width == 30 && m.height == 20 && m.count == 2);
    // the largest first: the room of 48 cells 1, the one of 30 cells 2; the closet (under kLeastCells) and the
    // filler none; no ground none
    CHECK(at(m.room, 20, 12) == 1 && at(m.room, 25, 15) == 1);
    CHECK(at(m.room, 2, 2) == 2 && at(m.room, 7, 6) == 2);
    CHECK(at(m.room, 12, 2) == 0 && at(m.room, 3, 13) == 0 && at(m.room, 15, 18) == 0);
    // their floors' heights (the middle one of each)
    CHECK(m.floor.size() == 3 && std::fabs(m.floor[1] - 4.0f) < 1e-4f && std::fabs(m.floor[2] - 10.0f) < 1e-4f);
    // widened kWiden cells: a wall's model stands on the cells beside its room's floor
    CHECK(at(m.widened, 0, 2) == 2 && at(m.widened, 9, 4) == 2 && at(m.widened, 4, 8) == 2 && at(m.widened, 10, 4) == 0);
    CHECK(at(m.widened, 16, 12) == 1 && at(m.widened, 15, 12) == 0);
    CHECK(at(m.widened, 2, 2) == 2);  // a room's own cells its own
    // the room at a place of the map's file (its middle at 0, 0; a cell 10 units): on its floor, beside it within
    // the widening, far from any
    CHECK(rooms::roomAt(m, (20.5f - 15.0f) * 10.0f, (12.5f - 10.0f) * 10.0f) == 1);
    CHECK(rooms::roomAt(m, (3.5f - 15.0f) * 10.0f, (3.5f - 10.0f) * 10.0f) == 2);
    CHECK(rooms::roomAt(m, (8.5f - 15.0f) * 10.0f, (3.5f - 10.0f) * 10.0f) == 2);
    CHECK(rooms::roomAt(m, (14.5f - 15.0f) * 10.0f, (18.5f - 10.0f) * 10.0f) == 0);
    CHECK(rooms::roomAt(m, 5000.0f, 0.0f) == 0);
    // a hall of one floor: one room - nothing to hide; no ground: none
    gnd::Ground hall = house();
    for (gnd::Cell& c : hall.cells) c.top = 0;
    CHECK(rooms::build(hall).count == 1);
    CHECK(rooms::build(gnd::Ground{}).count == 0);
}

// beta.52's ceiling stood over the map's lowest floors - in payon_in01 the black base at 0, its rooms' floors 6 to
// 17 up: a ceiling at 32 hung 22 over a room's floor, under the 3D view's camera (25 over the feet). Each room's
// ceiling over its own floor
TEST(cave_vault_a_ceiling_over_each_rooms_own_floor) {
    const gnd::Ground g = house();
    ground_heights::Ground heights;
    heights.width = g.width, heights.height = g.height, heights.zoom = g.zoom;
    for (const gnd::Cell& c : g.cells) heights.corners.insert(heights.corners.end(), c.h, c.h + 4);
    const rooms::Map m = rooms::build(g);
    cave_vault::Vault v;
    CHECK(cave_vault::buildCeiling(heights, 30.0f, &m, v));
    CHECK(v.perRoom && v.width == 30 && v.height == 20);
    const auto up = [&](int x, int y) { return v.up[size_t(y) * 30 + size_t(x)]; };
    // the walls' height by the models (30) and kCeilingOver over each room's floor
    CHECK(std::fabs(up(20, 12) - (4.0f + 30.0f + cave_vault::kCeilingOver)) < 1e-3f);
    CHECK(std::fabs(up(3, 3) - (10.0f + 30.0f + cave_vault::kCeilingOver)) < 1e-3f);
    CHECK(std::fabs(up(9, 4) - up(3, 3)) < 1e-3f);    // beside a room: its room's
    CHECK(up(15, 18) >= 30.0f);                       // far from any: no lower than a room's
    // no walls' height known: the usual room over each floor
    CHECK(cave_vault::buildCeiling(heights, 0.0f, &m, v));
    CHECK(std::fabs(up(20, 12) - (4.0f + cave_vault::kCeilingFlat)) < 1e-3f);
    // no rooms known: as before, one ceiling for the map
    CHECK(cave_vault::buildCeiling(heights, 30.0f, nullptr, v) && !v.perRoom);
}

TEST(config_view3d_one_room_on_by_default_and_live) {
    CHECK(parseConfig("").effects.oneRoom3d);
    CHECK(!parseConfig("[view3d]\none_room = 0\n").effects.oneRoom3d);
    const settings::IniSetting* row = settings::findSetting("view3d.one_room");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
    CHECK(appliesNow("view3d.one_room"));
}
