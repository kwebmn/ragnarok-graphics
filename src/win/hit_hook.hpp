#pragma once

namespace uagfx::win {

// 0.96: the game's sound at a place (uaRO.exe 0x4396B0: a file's name and where it is from the player) heard first
// by the driver - a hit's sound puts a flash where it lands (core/hit_flash.hpp, Device7::record). False, with a log
// line, when the exe is another build.
bool installHitHook();

}  // namespace uagfx::win
