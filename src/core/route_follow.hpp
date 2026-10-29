#pragma once
#include <string>

#include "core/reference_route.hpp"

namespace uagfx::routefollow {

// 3.0.11-beta.9 (the owner: «сделай по максимум»; the concept: the way on the minimap - a line to the next move, no
// walking for the player): the way chosen in a map's card («вести ›») - to a map, on foot or by Kafra. Made again from
// wherever the player is whenever the map or the walkable part there changes (a teleport, a death, a wrong turn: the
// way just starts there); its first move is the one shown. No Windows here.

struct Follow {
    bool active = false;
    std::string target;
    bool walk = true;
    std::string map;  // where the way was made from
    int part = -2;    // the player's walkable part then (-1 not known, -2 not made yet)
    int x = -1, y = -1;  // the player's cell then: made again further than kRemakeCells (of moves alike, the nearest)
    refroute::Route route;
};
constexpr int kRemakeCells = 40;

// The player's walkable part on a map: -1 the map's cells not known, or nothing walkable near (x, y)
int partOf(const refindex::Index& ix, const std::string& map, int x, int y);
// The player now on `map` at (x, y) (-1 not known): the way made again when the map or the part is another, or the
// player went far; true then
bool update(Follow& f, const refindex::Index& ix, const std::string& map, int x, int y);
// The move to take now: the route's first, when it starts on the player's map; null none (there, or no way)
const refroute::Step* next(const Follow& f, const std::string& map);

}  // namespace uagfx::routefollow
