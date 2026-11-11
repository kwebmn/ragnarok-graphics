#pragma once

namespace uagfx::win {

// 3.0.18-beta.174 (R-ZGN2US): an item the server says is worn at more than three of the game's ten places, or only at
// places it does not know, no longer crashes the game - its two calls of the game's wearing (core/equip_exe.hpp)
// through a guard that leaves such an item out (told in the log). False: not this build of uaRO.exe.
bool installWearGuard();

}  // namespace uagfx::win
