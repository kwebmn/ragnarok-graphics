#pragma once
#include <string>

namespace uagfx::mercai {

// 3.0.11-beta.217 (the owner hiring a mercenary: «AI.lua error - cannot read ./AI/AI_M.lua», then «attempt to call a
// nil value»): uaRO.exe runs the mercenary's AI from ./AI/AI_M.lua (/merai: ./AI/USER_AI/AI_M.lua) and calls its global
// AI(myid) every tick. The uaRO client ships no such file, and the uaRO server leads mercenaries itself (it takes no
// orders for them): an AI doing nothing answers the game's call. Written where the file is missing or defines no AI; a
// file with an AI of its own stays.
bool needsOurs(const std::string& text);
std::string text();

}  // namespace uagfx::mercai
