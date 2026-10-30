// 3.0.11-beta.31: the way across a map over the walked cells (core/walk_path) - round a wall, to an NPC standing on
// its cell, never across a wall's corner, its corners cut where a straight line keeps to walkable cells
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/walk_path.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// '#' a wall, '.' walked on; the first line is the north (the highest y)
walkpath::Cells cellsFrom(const std::vector<std::string>& rows) {
    const int h = int(rows.size()), w = int(rows[0].size());
    std::vector<uint8_t> walk(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) walk[size_t(y) * size_t(w) + size_t(x)] = rows[size_t(h - 1 - y)][size_t(x)] == '.';
    return walkpath::cellsOf(walkgrid::fromCells(w, h, walk));
}

bool allWalked(const walkpath::Cells& c, const std::vector<walkpath::Cell>& path) {
    for (const walkpath::Cell& p : path)
        if (!c.at(p.first, p.second)) return false;
    return true;
}

}  // namespace

TEST(walk_path_round_a_wall) {
    const walkpath::Cells c = cellsFrom({
        "..........",  // y 4: the gap over the wall
        ".....#....",
        ".....#....",
        ".....#....",
        ".....#....",  // y 0
    });
    CHECK(c.w == 10 && c.h == 5 && !c.at(5, 0) && c.at(5, 4));
    const walkpath::Field f = walkpath::fieldTo(c, {9, 0});
    CHECK_EQ(f.at(9, 0), 0u);
    CHECK(f.at(5, 0) == walkpath::kFar);  // the wall
    const std::vector<walkpath::Cell> path = walkpath::pathFrom(f, c, {0, 0});
    CHECK(path.size() > 10 && path.front() == walkpath::Cell(0, 0) && path.back() == walkpath::Cell(9, 0));
    CHECK(allWalked(c, path));
    bool over = false;  // it goes over the wall's top, never through it
    for (const walkpath::Cell& p : path) over = over || (p.first == 5 && p.second == 4);
    CHECK(over);
    const std::vector<walkpath::Cell> straight = walkpath::straightened(c, path);
    CHECK(straight.size() >= 3 && straight.size() < path.size());
    CHECK(straight.front() == path.front() && straight.back() == path.back());
    for (size_t i = 0; i + 1 < straight.size(); ++i) CHECK(walkpath::clear(c, straight[i], straight[i + 1]));
    CHECK(!walkpath::clear(c, {0, 0}, {9, 0}));  // straight through the wall: no
    CHECK(walkpath::clear(c, {0, 4}, {9, 4}));
}

TEST(walk_path_to_an_npc_and_no_way) {
    const walkpath::Cells c = cellsFrom({
        "......",
        "..#...",  // y 2: the NPC's cell (x 2) a wall
        "......",
        "###.##",  // y 0: x 3 walked, joined to the row above
    });
    const walkpath::Field f = walkpath::fieldTo(c, {2, 2});
    const std::vector<walkpath::Cell> path = walkpath::pathFrom(f, c, {5, 3});
    CHECK(!path.empty() && allWalked(c, path));
    const walkpath::Cell end = path.back();  // beside the NPC
    CHECK(std::abs(end.first - 2) <= 1 && std::abs(end.second - 2) <= 1 && !(end == walkpath::Cell(2, 2)));
    // a room shut all round: no way out of it
    const walkpath::Cells shut = cellsFrom({
        ".....",
        ".###.",
        ".#.#.",
        ".###.",
        ".....",
    });
    const walkpath::Field g = walkpath::fieldTo(shut, {0, 0});
    CHECK(walkpath::pathFrom(g, shut, {2, 2}).empty());
    CHECK(!walkpath::pathFrom(g, shut, {4, 4}).empty());
}

TEST(walk_path_no_corner_cutting) {
    const walkpath::Cells c = cellsFrom({
        "#.",  // (0, 1) a wall, (1, 1) walked
        ".#",  // (0, 0) walked, (1, 0) a wall
    });
    const walkpath::Field f = walkpath::fieldTo(c, {1, 1});
    CHECK(f.at(0, 0) == walkpath::kFar);  // only a diagonal past two walls' corners - none
    CHECK(walkpath::pathFrom(f, c, {0, 0}).empty());
    CHECK(!walkpath::clear(c, {0, 0}, {1, 1}));
    const walkpath::Cells open = cellsFrom({"..", ".."});
    const walkpath::Field o = walkpath::fieldTo(open, {1, 1});
    CHECK_EQ(o.at(0, 0), 14u);  // one diagonal: 1.4 cells
    CHECK_EQ(walkpath::pathFrom(o, open, {0, 0}).size(), size_t(2));
}

// 3.0.11-beta.34 (the owner: «каждый шаг перестраивается маршрут», «каждый раз перестраивается линия, можно так чтобы
// она не очень дергалась»): the way kept while the player keeps to it - where on it the player is, what is left of it
TEST(walk_path_place_on_the_way) {
    const std::vector<std::pair<float, float>> way{{0, 0}, {10, 0}, {10, 10}};  // east 10, then north 10
    walkpath::OnWay p = walkpath::onWay(way, 4, 1, 0, 2, 8);
    CHECK(std::fabs(p.at - 4) < 1e-4f && std::fabs(p.off - 1) < 1e-4f && p.leg == 0);
    CHECK(std::fabs(p.x - 4) < 1e-4f && std::fabs(p.y) < 1e-4f);
    // looked for no farther than `ahead` past where the player was: (10, 6) is 16 along, the window ends at 8
    p = walkpath::onWay(way, 10, 6, 0, 2, 8);
    CHECK(std::fabs(p.at - 8) < 1e-4f && std::fabs(p.off - std::hypot(2.0f, 6.0f)) < 1e-4f);
    p = walkpath::onWay(way, 10, 6, 12, 2, 8);
    CHECK(std::fabs(p.at - 16) < 1e-4f && p.off < 1e-4f && p.leg == 1);
    // a way that comes back near itself: the leg back (2 cells over) is not jumped to while the player is on the first
    const std::vector<std::pair<float, float>> u{{0, 0}, {0, 10}, {2, 10}, {2, 0}};
    p = walkpath::onWay(u, 1.6f, 3, 3, 2, 8);
    CHECK(std::fabs(p.at - 3) < 1e-4f && std::fabs(p.off - 1.6f) < 1e-4f && p.leg == 0);
    p = walkpath::onWay(u, 1.6f, 3, 3, 2, 30);  // looked for far enough: the nearer leg back
    CHECK(std::fabs(p.at - 19) < 1e-4f && std::fabs(p.off - 0.4f) < 1e-4f && p.leg == 2);
    // a place as near on two legs (a corner): the earlier
    p = walkpath::onWay(way, 10, 0, 0, 2, 30);
    CHECK(std::fabs(p.at - 10) < 1e-4f && p.leg == 0);
    CHECK(walkpath::onWay({}, 1, 1, 0, 2, 8).off < 0);  // no way
    p = walkpath::onWay({{3, 4}}, 0, 0, 0, 2, 8);         // a way of one place
    CHECK(p.at == 0 && std::fabs(p.off - 5) < 1e-4f);
}

TEST(walk_path_the_rest_of_the_way) {
    const std::vector<walkpath::Cell> way{{0, 0}, {10, 0}, {10, 10}};
    walkpath::OnWay p;
    p.leg = 0;
    p.off = 1;
    CHECK(walkpath::rest(way, {4, 1}, p) == std::vector<walkpath::Cell>({{4, 1}, {10, 0}, {10, 10}}));
    p.leg = 1;
    CHECK(walkpath::rest(way, {9, 5}, p) == std::vector<walkpath::Cell>({{9, 5}, {10, 10}}));
    p.leg = 0;  // the player on the corner itself: once
    CHECK(walkpath::rest(way, {10, 0}, p) == std::vector<walkpath::Cell>({{10, 0}, {10, 10}}));
}

TEST(walk_path_kept_while_followed) {
    const walkpath::Cells c = cellsFrom({
        "....................",  // y 9
        "....................",  // y 8: the gap over the wall
        "..........#.........",
        "..........#.........",
        "..........#.........",
        "..........#.........",
        "..........#.........",
        "..........#.........",
        "..........#.........",
        "..........#.........",  // y 0
    });
    const walkpath::Field f = walkpath::fieldTo(c, {18, 1});
    walkpath::Kept k;
    CHECK(walkpath::keep(k, f, c, {2, 1}, 3.0f));  // made: from the player, straightened
    CHECK(k.way.size() >= 3 && k.way.front() == walkpath::Cell(2, 1) && k.way.back() == walkpath::Cell(18, 1));
    CHECK(k.rest == k.way && k.at == 0);
    const std::vector<walkpath::Cell> made = k.way;
    // a step along it, then one aside of it: the same way, cut at the player
    const walkpath::Cell second = made[1];
    const walkpath::Cell along{2 + (second.first - 2) / 2, 1 + (second.second - 1) / 2};
    CHECK(!walkpath::keep(k, f, c, along, 3.0f));
    CHECK(k.way == made && k.at > 0 && k.off < 1.0f);
    CHECK(k.rest.front() == along && k.rest.back() == walkpath::Cell(18, 1) && k.rest.size() == made.size());
    const walkpath::Cell aside{along.first + 1, along.second};
    CHECK(!walkpath::keep(k, f, c, aside, 3.0f) && k.way == made && k.rest.front() == aside);
    // far off it: made again from there
    CHECK(walkpath::keep(k, f, c, {2, 9}, 3.0f));
    CHECK(k.way.front() == walkpath::Cell(2, 9) && k.way.back() == walkpath::Cell(18, 1) && k.at == 0);
    // another goal: made again
    const walkpath::Cell from = k.way.front();
    CHECK(walkpath::keep(k, walkpath::fieldTo(c, {18, 9}), c, from, 3.0f));
    CHECK(k.way.back() == walkpath::Cell(18, 9));
    // no way to the goal over the cells (none known, or none reach it): a straight line to it, kept as any
    const walkpath::Field none = walkpath::fieldTo(c, {50, 50});
    CHECK(walkpath::keep(k, none, c, {2, 1}, 3.0f));
    CHECK(k.way == std::vector<walkpath::Cell>({{2, 1}, {50, 50}}) && k.rest == k.way);
    CHECK(!walkpath::keep(k, none, c, {3, 2}, 3.0f) && k.way.front() == walkpath::Cell(2, 1));
    const walkpath::Cells unknown;
    CHECK(walkpath::keep(k, walkpath::fieldTo(unknown, {7, 7}), unknown, {1, 1}, 3.0f));
    CHECK(k.way == std::vector<walkpath::Cell>({{1, 1}, {7, 7}}));
}

// 3.0.11-beta.71 (the owner: «хочется на карте прокладывать путь»): a way through the points set on the map, in turn -
// each leg over the walked cells from the last point; its corners in order, the points among them
TEST(walk_path_through_points) {
    const walkpath::Cells c = cellsFrom({
        "..........",  // y 4: the gap over the wall
        ".....#....",
        ".....#....",
        ".....#....",
        ".....#....",  // y 0
    });
    const std::vector<walkpath::Cell> way = walkpath::through(c, {0, 0}, {{9, 0}, {9, 3}});
    CHECK(way.size() >= 4 && way.front() == walkpath::Cell(0, 0) && way.back() == walkpath::Cell(9, 3));
    CHECK(allWalked(c, way));
    CHECK(std::find(way.begin(), way.end(), walkpath::Cell(9, 0)) != way.end());  // through the first point
    bool over = false;  // round the wall by its gap at y 4
    for (const walkpath::Cell& p : way) over = over || p.second == 4;
    CHECK(over);
    for (size_t i = 1; i < way.size(); ++i) CHECK(walkpath::clear(c, way[i - 1], way[i]));  // each leg walkable
    // a point on a wall: reached from beside it; none walkable at all: a straight line to it
    const std::vector<walkpath::Cell> wall = walkpath::through(c, {0, 0}, {{5, 1}});
    CHECK(wall.size() >= 2 && wall.front() == walkpath::Cell(0, 0) && std::abs(wall.back().first - 5) <= 1);
    CHECK(walkpath::through(walkpath::Cells{}, {0, 0}, {{3, 3}}) ==
          std::vector<walkpath::Cell>({walkpath::Cell(0, 0), walkpath::Cell(3, 3)}));
    CHECK(walkpath::through(c, {0, 0}, {}).empty());
}
