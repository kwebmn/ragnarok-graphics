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

// 3.0.11-beta.124 (the network plan's step 4): a move come late (the game walks the unit on from where it should be by
// now - a jump forward) or from a cell other than the shown one: the jump drawn away over a moment - an offset from the
// game's own place fading out
TEST(net_smooth_a_moves_jump_fades) {
    CHECK(!netsmooth::easesJump(0.2f) && netsmooth::easesJump(0.5f) && netsmooth::easesJump(4.0f));
    CHECK(!netsmooth::easesJump(4.5f));  // farther: a real jump
    netsmooth::Offset o;
    o.offX = 10;
    o.offZ = -5;
    o.startMs = 1000;
    o.durationMs = 200;
    float x = 0, z = 0;
    CHECK(netsmooth::offsetAt(o, 1000, x, z) && x == 10 && z == -5);  // at first: where it was shown
    CHECK(netsmooth::offsetAt(o, 1100, x, z) && x > 0 && x < 5);      // fading (eased out)
    CHECK(!netsmooth::offsetAt(o, 1200, x, z) && x == 0 && z == 0);   // gone: the game's own place
}

// 3.0.11-beta.251 (R-UCE7J8, R-YB4S2K «моб бьет стоя гдето далеко»): a unit the game left standing off its
// server's cell
TEST(net_smooth_stranded_units) {
    CHECK(!netsmooth::stranded(139, 182, 132, 177, netsmooth::kStrandMs - 1));  // not long enough
    CHECK(netsmooth::stranded(139, 182, 132, 177, netsmooth::kStrandMs));
    CHECK(!netsmooth::stranded(132, 177, 132, 177, 5000));  // on its cell: standing as it should
    CHECK(netsmooth::easesFar(7) && !netsmooth::easesFar(13) && !netsmooth::easesFar(0.2f));
    CHECK(netsmooth::farFadeMs(7) > netsmooth::farFadeMs(1) && netsmooth::farFadeMs(30) == netsmooth::farFadeMs(10));
}
