#pragma once
#include <string>

namespace uagfx {

// 0.33: whether a map lies under the open sky - a field or a town - by its name: the open sky's effects go over it.
// Dungeons, houses, towers, castles, instances and the like do not.
bool outdoors(const std::string& map);
// 2.6.10: whether the game's own fog is drawn - as the state asks, but under the open sky with no_game_fog
inline bool gameFogDrawn(bool stateFog, bool noGameFog, bool openSky) { return stateFog && !(noGameFog && openSky); }
// 0.35: the desert - Morroc, its fields and its ruins (not the pyramids inside) - shimmers far off in the heat.
bool desert(const std::string& map);
// 2.5.17: how much a map's floor under a roof reflects what stands on it (the floor reflections), 0-1: smooth floors
// (castles, churches, palaces, towers, labs, temples, ice) 1, houses' floors 0.5, a dungeon's earth and rock 0 - a
// cave's cracked floor showed smeared copies of the loot (anthell01). Under the open sky 0 (the rain decides there).
float floorShine(const std::string& map);

}  // namespace uagfx
