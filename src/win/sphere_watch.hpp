#pragma once
#include <cstdint>

namespace uagfx::win::spherewatch {

// 3.0.16-beta.8 (the owner: «для монков например еще и сферы»): the character's own spirit spheres (a Gunslinger's
// coins) as the server tells them - 0x01d0 (the area, a change) and 0x01e1 (to the character alone, after a map's
// loading when some are up): <unit>.L <count>.W, the unit the session's own account; a map change (0x0091, 0x0092)
// none until told again (uAthena sends nothing for none). On the game's thread.
void packet(uint16_t id, const uint8_t* packet);
int count();  // up now (0 none or out of the world)

}  // namespace uagfx::win::spherewatch
