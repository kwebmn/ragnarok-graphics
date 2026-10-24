#pragma once

namespace uagfx::win {

// 3.0.4-beta.9 (R-VBDSCP): a character's shadow drawn while a map loads no longer crashes the game - its call for the
// game mode through a guard (core/shadow_exe.hpp). False: not this build of uaRO.exe.
bool installShadowFix();

}  // namespace uagfx::win
