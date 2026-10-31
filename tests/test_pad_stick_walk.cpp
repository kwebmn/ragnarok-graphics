// 3.0.11-beta.102 (the owner: «проработал передвижение с контролером, чтобы стрелочка показывалась как в концепции и
// оно не глючило при хождении. ранее у нас курсор мышки просто тыкал на клетку, если маштаб маленький то были рывки
// при хождение а если большой маштаб то шел дальше чем нужно»): walking by the stick in the map's cells - the way the
// stick points with the camera's turn, how far ahead the cell asked for is (by the walking speed, the same at any
// zoom), the walls stopped at or slid along, when to ask again (never arriving, so never stopping), where letting go
// stops
#include <cmath>
#include <optional>
#include <utility>

#include "pad/core/logic/stick_walk.hpp"
#include "test.hpp"

using namespace uapad;

namespace {

GroundGrid open() {
    GroundGrid g;
    g.w = g.h = 200;
    g.walk.assign(size_t(200) * 200, 1);
    return g;
}

void wall(GroundGrid& g, int x) {  // a column of unwalkable cells at x
    for (int y = 0; y < g.h; ++y) g.walk[size_t(y) * size_t(g.w) + size_t(x)] = 0;
}

}  // namespace

TEST(stick_walk_the_way_turns_with_the_camera) {
    stickwalk::Way w = stickwalk::wayOf(1, 0, std::make_pair(0.0, 1.0));  // the camera looks north: right is east
    CHECK(std::fabs(w.east - 1) < 1e-9 && std::fabs(w.north) < 1e-9);
    w = stickwalk::wayOf(0, 1, std::make_pair(1.0, 0.0));  // it looks east: up is east
    CHECK(std::fabs(w.east - 1) < 1e-9 && std::fabs(w.north) < 1e-9);
    w = stickwalk::wayOf(1, 0, std::make_pair(1.0, 0.0));  // and right is south
    CHECK(std::fabs(w.east) < 1e-9 && std::fabs(w.north + 1) < 1e-9);
    w = stickwalk::wayOf(0.3, 0.3, std::nullopt);  // no camera known: north up; a unit whatever the push
    CHECK(std::fabs(std::hypot(w.east, w.north) - 1) < 1e-9 && std::fabs(w.east - w.north) < 1e-9);
    CHECK(std::fabs(stickwalk::turnBetween(stickwalk::Way{1, 0}, stickwalk::Way{0, 1}) - 3.14159265358979 / 2) <
          1e-6);
}

TEST(stick_walk_asks_ahead_by_the_walking_speed) {
    CHECK_EQ(stickwalk::lead(150), 5);  // about 0.6 s of walking and a cell
    CHECK_EQ(stickwalk::lead(200), 4);
    CHECK_EQ(stickwalk::lead(100), 6);  // fast: no further than 6
    CHECK_EQ(stickwalk::lead(400), 3);  // slow: no nearer than 3
    CHECK_EQ(stickwalk::lead(0), 5);    // not known: the game's default
}

TEST(stick_walk_the_cell_along_the_way_and_the_walls) {
    GroundGrid g = open();
    const Cell here{100, 100};
    CHECK(stickwalk::destination(here, stickwalk::Way{1, 0}, 5, &g) == (Cell{105, 100}));
    const double d = std::sqrt(0.5);
    CHECK(stickwalk::destination(here, stickwalk::Way{d, d}, 5, &g) == (Cell{104, 104}));  // 5 cells' way, slant
    CHECK(stickwalk::destination(here, stickwalk::Way{1, 0}, 5, nullptr) == (Cell{105, 100}));  // no cells known
    wall(g, 104);
    CHECK(stickwalk::destination(here, stickwalk::Way{1, 0}, 5, &g) == (Cell{103, 100}));  // short of the wall
}

TEST(stick_walk_slides_along_a_wall_ahead) {
    GroundGrid g = open();
    wall(g, 101);  // right in front
    const double a = 30.0 * 3.14159265358979 / 180.0;  // the stick a little north of east
    const Cell to = stickwalk::destination(Cell{100, 100}, stickwalk::Way{std::cos(a), std::sin(a)}, 5, &g);
    CHECK(to.x == 100 && to.y > 101);  // along the wall, the way's side of it
    const Cell back = stickwalk::destination(Cell{100, 100}, stickwalk::Way{std::cos(-a), std::sin(-a)}, 5, &g);
    CHECK(back.x == 100 && back.y < 99);
    GroundGrid boxed = open();
    for (int x = 99; x <= 101; ++x)
        for (int y = 99; y <= 101; ++y)
            if (x != 100 || y != 100) boxed.walk[size_t(y) * 200 + size_t(x)] = 0;
    CHECK(stickwalk::destination(Cell{100, 100}, stickwalk::Way{1, 0}, 5, &boxed) == (Cell{100, 100}));  // nowhere
}

TEST(stick_walk_letting_go_stops_at_the_next_cell) {
    GroundGrid g = open();
    CHECK(stickwalk::stopCell(Cell{100, 100}, stickwalk::Way{1, 0}, &g) == (Cell{101, 100}));
    const double d = std::sqrt(0.5);
    CHECK(stickwalk::stopCell(Cell{100, 100}, stickwalk::Way{-d, d}, &g) == (Cell{99, 101}));
    wall(g, 101);
    CHECK(stickwalk::stopCell(Cell{100, 100}, stickwalk::Way{1, 0}, &g) == (Cell{100, 100}));  // a wall: where it is
}

TEST(stick_walk_asks_again_before_arriving) {
    const stickwalk::Way east{1, 0};
    CHECK(stickwalk::askAgain(std::nullopt, Cell{100, 100}, Cell{105, 100}, east, 5, 1000));  // the first ask
    const stickwalk::Asked last{Cell{105, 100}, east, 1000};
    CHECK(!stickwalk::askAgain(last, Cell{101, 100}, Cell{106, 100}, east, 5, 1200));  // far from it yet
    CHECK(stickwalk::askAgain(last, Cell{103, 100}, Cell{108, 100}, east, 5, 1400));   // halfway: on before arriving
    const double a = 30.0 * 3.14159265358979 / 180.0;
    const stickwalk::Way turned{std::cos(a), std::sin(a)};
    CHECK(stickwalk::askAgain(last, Cell{101, 100}, Cell{105, 103}, turned, 5, 1300));   // turned past 22.5°
    CHECK(!stickwalk::askAgain(last, Cell{101, 100}, Cell{105, 103}, turned, 5, 1050));  // not sooner than 100 ms
    const double b = 10.0 * 3.14159265358979 / 180.0;
    CHECK(!stickwalk::askAgain(last, Cell{101, 100}, Cell{106, 101}, stickwalk::Way{std::cos(b), std::sin(b)}, 5,
                               1300));  // a little: the same way
    CHECK(!stickwalk::askAgain(last, Cell{104, 100}, Cell{105, 100}, east, 5, 1600));  // the same cell (a wall)
    CHECK(stickwalk::askAgain(last, Cell{101, 100}, Cell{106, 100}, east, 5, 2100));  // a second on: again anyway
    // the cell along the way moved aside (round a post, along a wall) with the stick held as it was: asked at once
    CHECK(stickwalk::askAgain(last, Cell{100, 100}, Cell{103, 103}, east, 5, 1300));
}
