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
// 3.0.18-beta.252 (R-9UNG8S, the owner on prtg_cas01: «в замке трава плоская с отражением»): a guild castle's map
// (prtg_cas, payg_cas, gefg_cas, aldeg_cas, schg_cas, arug_cas, the novices' nguild_) - mostly its courtyards' grass
// under the sky, its halls (the Emperium's) inside the same map. Under a roof for the rest (outdoors false): no
// weather, no sky's light, its halls' floors reflecting.
bool castleGrounds(const std::string& map);
// 3.0.18-beta.252: where our grass grows - under the open sky, and on a castle's grounds; there only off its halls'
// floors (ground_heights::interiorTexture), so only once its ground is read and placed (the renderer's)
bool grassGrows(const std::string& map);

}  // namespace uagfx
