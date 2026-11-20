#pragma once

namespace uagfx::win {

// 3.0.22-beta.31 (R-JBG5MM): a same-map teleport of a level 99 character no longer crashes the game - the call that
// shows the world's player its icons and aura again goes through a guard (core/aura_exe.hpp) that skips it while the
// world has no player. False: not this build of uaRO.exe.
bool installAuraFix();

}  // namespace uagfx::win
