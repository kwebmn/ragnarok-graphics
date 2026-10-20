#pragma once

namespace uagfx::win {

// 2.6.13: once a frame on the game's thread - with `leaveOut` (effects.no_server_night) the darkness uaRO.exe puts on
// the world for the server's night taken away, given back when it is no longer left out (core/server_night.hpp);
// the game's own when the exe is another build.
void serverNightFrame(bool leaveOut);

}  // namespace uagfx::win
