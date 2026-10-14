#pragma once
#include <string>

namespace uagfx::haze {

// 0.34, haze and fog (win/gpu/haze.cpp), by the kind of map: under the open sky a light bluish haze far off; in the
// dungeons the dark far off and a low fog drifting over the ground; in houses, towers, instances and the like none.
struct Look {
    float haze = 0.0f;  // how much far off, 0-1 (before the player's setting)
    float hazeColour[3] = {0, 0, 0};
    float hazeStart = 200.0f, hazeEnd = 1100.0f;  // view depth, world units
    float fog = 0.0f;  // over the ground, 0-1
    float fogColour[3] = {0, 0, 0};
};

Look lookOf(const std::string& map);

}  // namespace uagfx::haze
