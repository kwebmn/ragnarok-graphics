#pragma once
#include <string>

namespace uagfx {

// 0.33: whether a map lies under the open sky - a field or a town - by its name: the clouds' shadows drift over it.
// Dungeons, houses, towers, castles, instances and the like do not.
bool outdoors(const std::string& map);
// 0.35: the desert - Morroc, its fields and its ruins (not the pyramids inside) - shimmers far off in the heat.
bool desert(const std::string& map);

}  // namespace uagfx
