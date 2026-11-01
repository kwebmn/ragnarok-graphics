// 3.0.11-beta.145 (the owner's concept «маг, прист и хант на геймпаде», answers 1 and 2): a ground skill's button held
// - the aim (pad/core/logic/ground_aim): walls and traps by the side the stick leans to, areas and zones as a cursor
// that sticks to a monster or a friend; the D-pad a cell; never past the skill's reach
#include <cmath>
#include <vector>

#include "pad/core/logic/ground_aim.hpp"
#include "test.hpp"

using namespace uapad;

namespace {

constexpr double kDiag = 0.70710678;

}  // namespace

TEST(ground_aim_style_by_skill) {
    CHECK(GroundAim::styleOf(18) == GroundAim::Style::Side);   // Fire Wall
    CHECK(GroundAim::styleOf(87) == GroundAim::Style::Side);   // Ice Wall
    CHECK(GroundAim::styleOf(117) == GroundAim::Style::Side);  // Ankle Snare
    CHECK(GroundAim::styleOf(80) == GroundAim::Style::Side);   // Fire Pillar
    CHECK(GroundAim::styleOf(89) == GroundAim::Style::Cursor);  // Storm Gust
    CHECK(GroundAim::styleOf(70) == GroundAim::Style::Cursor);  // Sanctuary
}

// The side the stick leans to, in the server's own 8; how far by the lean - 1 cell just past the dead zone, 4 at full
// (a trap's 3); let go - it stays; the D-pad moves it a cell
TEST(ground_aim_wall_by_the_side) {
    const Cell me{100, 100};
    GroundAim a;
    a.start(18, 10, 0, 0x4000, me, Cell{102, 100}, 9);
    CHECK(a.active() && a.cell() == (Cell{102, 100}));  // from the press's own cell
    a.step(1.0, 0.0, Cell{0, 0}, me, {}, 16);
    CHECK(a.cell() == (Cell{104, 100}));
    a.step(kDiag, kDiag, Cell{0, 0}, me, {}, 16);  // up-right on the map: the diagonal
    CHECK(a.cell() == (Cell{104, 104}));
    a.step(0.675, 0.0, Cell{0, 0}, me, {}, 16);  // half way between the dead zone and a full lean
    CHECK(a.cell() == (Cell{103, 100}));
    a.step(0.4, 0.0, Cell{0, 0}, me, {}, 16);
    CHECK(a.cell() == (Cell{101, 100}));
    a.step(0.0, 0.0, Cell{0, 0}, me, {}, 16);  // let go
    CHECK(a.cell() == (Cell{101, 100}));
    a.step(0.0, 0.0, Cell{0, 1}, me, {}, 16);  // the D-pad up on the map
    CHECK(a.cell() == (Cell{101, 101}));
    a.stop();
    CHECK(!a.active());
    GroundAim t;
    t.start(117, 5, 1, 0x1000, me, Cell{102, 100}, 3);  // Ankle Snare: 3 at most
    t.step(-1.0, 0.0, Cell{0, 0}, me, {}, 16);
    CHECK(t.cell() == (Cell{97, 100}));
}

// Never past the reach: the D-pad held east from 9 cells away
TEST(ground_aim_within_reach) {
    const Cell me{100, 100};
    GroundAim a;
    a.start(18, 10, 0, 0x4000, me, Cell{108, 100}, 9);
    for (int i = 0; i < 5; ++i) a.step(0.0, 0.0, Cell{1, 0}, me, {}, 16);
    CHECK(a.cell() == (Cell{109, 100}) && a.inReach(me));
}

// An area's cursor: up to 12 cells a second at a full lean; let go near a monster - on it; never past the reach
TEST(ground_aim_area_cursor_sticks) {
    const Cell me{100, 100};
    GroundAim a;
    a.start(89, 10, 2, 0x2000, me, Cell{100, 100}, 9);
    a.step(1.0, 0.0, Cell{0, 0}, me, {}, 500);
    CHECK(a.cell() == (Cell{106, 100}));
    a.step(0.0, 0.0, Cell{0, 0}, me, {Cell{107, 101}, Cell{90, 90}}, 16);
    CHECK(a.cell() == (Cell{107, 101}));
    a.step(1.0, 0.0, Cell{0, 0}, me, {}, 2000);
    CHECK(a.cell() == (Cell{109, 101}));
    a.step(0.0, 0.0, Cell{0, 0}, me, {Cell{100, 120}}, 16);  // nothing near: it stays
    CHECK(a.cell() == (Cell{109, 101}));
}
