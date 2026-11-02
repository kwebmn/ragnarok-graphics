#pragma once
#include <optional>
#include <string>

namespace uagfx::cameramemory {

// 3.0.11-beta.184 (R-ANMTEJ «Зум при вході в гру збивається ... зберігати останнє значення з гри»): uaRO.exe keeps the
// camera's tilt and distance in two slots of its .data, outdoors and buildings (core/game_camera.hpp kOutdoorTilt...):
// written at a map's exit, read at the next map's start - and -45, 300 at every launch. [camera] remember keeps them in
// uaro-graphics\camera.txt between the games (win/game_camera rememberCamera).
struct Pose {
    float tilt = -45.0f, zoom = 300.0f;
};
struct Memory {
    Pose outdoors, buildings;
};

// "outdoors -60.0 812.6\nbuildings -45.0 275.7\n"
std::string format(const Memory& m);
// Nothing when a slot's line is missing or not two numbers the game can hold (tilt -90..0, zoom 100..5000).
std::optional<Memory> parse(const std::string& text);
// The game's slots with the camera now put into the current map's own: a building's into `buildings`.
Memory withNow(Memory slots, Pose now, bool building);
// More than half a unit apart anywhere: worth writing again.
bool differs(const Memory& a, const Memory& b);

}  // namespace uagfx::cameramemory
