// 3.0.11-beta.123 (the network plan's step 3, the owner «начинай по очереди»): the server's position fixes (0x0088) of
// 1-3 cells glided to instead of jumped (core/net_smooth) - which, how long, the way along, the cell's middle
#include <cmath>

#include "core/minimap_proto.hpp"
#include "core/net_smooth.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(net_smooth_which_fixes_glide) {
    CHECK(!netsmooth::eases(0));  // already there
    CHECK(netsmooth::eases(1) && netsmooth::eases(3));
    CHECK(!netsmooth::eases(4));  // farther: a real jump (a teleport, a knock-back) - as before
    CHECK(netsmooth::glideMs(1) == 160 && netsmooth::glideMs(3) == 240);
}

TEST(net_smooth_the_way_along) {
    netsmooth::Glide g;
    g.fromX = 0;
    g.fromZ = 0;
    g.toX = 10;
    g.toZ = -20;
    g.startMs = 1000;
    g.durationMs = 200;
    float x = 0, z = 0;
    CHECK(netsmooth::at(g, 1000, x, z) && x == 0 && z == 0);
    CHECK(netsmooth::at(g, 1100, x, z) && x > 5 && x < 10 && z < -10 && z > -20);  // eased out: past half by then
    CHECK(!netsmooth::at(g, 1200, x, z) && x == 10 && z == -20);  // done: at its cell
    CHECK(netsmooth::eased(0) == 0 && netsmooth::eased(1) == 1 && netsmooth::eased(2) == 1);
}

TEST(net_smooth_a_cells_middle) {
    float x = 0, z = 0;
    netsmooth::cellMiddle(136, 62, 312, 392, 5, x, z);
    minimap::Cell c;
    CHECK(minimap::cellOf(x, z, 312, 392, 5, c) && c.x == 136 && c.y == 62);
    CHECK(std::fabs(x - (136 - 156 + 0.5f) * 5) < 0.01f);
}
