#pragma once

namespace uagfx::win {

// 3.0.13-beta.7: [interface] quick_teleport - a teleport on the same map without the game's 255 ms of darkening
// (core/teleport_fade_exe.hpp); each frame, the code changed only when the wish changes. Another build: a log line.
void quickTeleport(bool wanted);

}  // namespace uagfx::win
