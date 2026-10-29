#pragma once
#include <cstdint>
#include <utility>
#include <vector>

#include "core/walk_grid.hpp"

namespace uagfx::walkpath {

// 3.0.11-beta.31 (the owner: «маршрут на локации строится без учета препятствий, то есть прямая линия к порталу»):
// the way across a map to a cell - a warp's, an NPC's - over the cells one walks on (the server's, walkgrid::Grid):
// how far each cell is from the goal (eight ways, a diagonal 1.4 cells, never across a wall's corner), then from the
// player down that distance to it, its corners cut where a straight line keeps to walkable cells. No Windows here.

using Cell = std::pair<int, int>;  // x east, y north

// A map's cells flat, row by row from y 0: 1 walked on, 0 not
struct Cells {
    int w = 0, h = 0;
    std::vector<uint8_t> walk;
    bool at(int x, int y) const {
        return x >= 0 && y >= 0 && x < w && y < h && walk[size_t(y) * size_t(w) + size_t(x)] != 0;
    }
};
Cells cellsOf(const walkgrid::Grid& g);

constexpr uint32_t kFar = UINT32_MAX;  // a cell that cannot reach the goal
// How far each cell is from the goal, in tenths of a cell; a goal on a wall (an NPC stands on its cell) is reached
// from the walkable cells within `reach` of it, each from its own distance to it
struct Field {
    int w = 0, h = 0;
    Cell goal{-1, -1};
    std::vector<uint32_t> dist;
    uint32_t at(int x, int y) const {
        return x >= 0 && y >= 0 && x < w && y < h ? dist[size_t(y) * size_t(w) + size_t(x)] : kFar;
    }
};
Field fieldTo(const Cells& c, Cell goal, int reach = 4);
// The cells from `from` down to the goal's ground (both ends in); empty: `from` cannot reach it. A start on a wall
// (the player's cell not yet known as walked) goes from its nearest walkable neighbour
std::vector<Cell> pathFrom(const Field& f, const Cells& c, Cell from);
// Whether a straight line between two cells' middles keeps to walkable cells (through a corner: both its sides)
bool clear(const Cells& c, Cell a, Cell b);
// The path with fewer corners: from each point on, the farthest one a straight line reaches over walkable cells
std::vector<Cell> straightened(const Cells& c, const std::vector<Cell>& path);

// 3.0.11-beta.34 (the owner: «каждый шаг перестраивается маршрут», «каждый раз перестраивается линия, можно так чтобы
// она не очень дергалась»): the way kept while the player keeps to it, not made again from each cell they step on -
// it was straightened from the player's cell anew each step, its corners and the footprints along it jumping.
// Where on a way (its corners, any units) the player at (x, y) is: the nearest place on its legs, looked for from
// `from` (how far along it they were) `back` behind to `ahead` past it - a way that comes back near itself is not
// jumped along; as near on two legs: the earlier
struct OnWay {
    float at = 0;        // how far along the way
    float off = -1;      // from (x, y) to that place; -1: no way
    size_t leg = 0;      // the leg it is on: from corner `leg` to `leg + 1`
    float x = 0, y = 0;  // the place
};
OnWay onWay(const std::vector<std::pair<float, float>>& way, float x, float y, float from, float back, float ahead);
// What is left of a way for the player on `player` at `p`: their cell, then the corners past their place
std::vector<Cell> rest(const std::vector<Cell>& way, Cell player, const OnWay& p);
// The way kept for the player: made again (pathFrom the player, straightened; none over the cells - none known, none
// reaching the goal - a straight line to it) for another goal or when the player is farther than `off` cells from it
// (looked for kBack cells behind where they were to kAhead past it); true then
constexpr float kBack = 2, kAhead = 8;
struct Kept {
    std::vector<Cell> way;   // as made: from the player's cell then
    Cell goal{-1, -1};       // the field's it was made for
    float at = 0, off = 0;   // how far along it the player is, how far off it (cells)
    std::vector<Cell> rest;  // what is left of it (rest())
};
bool keep(Kept& k, const Field& f, const Cells& c, Cell player, float off);

}  // namespace uagfx::walkpath
