// 3.0.11-beta.31: the way across a map over the walked cells (core/walk_path) - round a wall, to an NPC standing on
// its cell, never across a wall's corner, its corners cut where a straight line keeps to walkable cells
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
