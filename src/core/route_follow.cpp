#include "core/route_follow.hpp"

#include <algorithm>
#include <cstdlib>

namespace uagfx::routefollow {

int partOf(const refindex::Index& ix, const std::string& map, int x, int y) {
    if (!ix.walk || x < 0 || y < 0) return -1;
    const auto it = ix.walk->find(map);
    if (it == ix.walk->end()) return -1;
    const std::vector<uint8_t> parts = it->second.around(x, y, 3);
    return parts.empty() ? -1 : parts.front();
}

bool update(Follow& f, const refindex::Index& ix, const std::string& map, int x, int y) {
    if (!f.active || map.empty()) return false;
    const int part = partOf(ix, map, x, y);
    const bool far = x >= 0 && f.x >= 0 && std::max(std::abs(x - f.x), std::abs(y - f.y)) > kRemakeCells;
    if (map == f.map && part == f.part && !far) return false;
    f.map = map;
    f.part = part;
    f.x = x;
    f.y = y;
    f.route = refroute::find(ix, map, x, y, f.target, f.walk);
    return true;
}

const refroute::Step* next(const Follow& f, const std::string& map) {
    if (!f.active || !f.route.found || f.route.steps.empty() || f.route.steps.front().from != map) return nullptr;
    return &f.route.steps.front();
}

}  // namespace uagfx::routefollow
