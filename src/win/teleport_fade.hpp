#pragma once

namespace uagfx::win {

// 3.0.13-beta.7: [interface] quick_teleport - a teleport on the same map without the game's 255 ms of darkening
// (core/teleport_fade_exe.hpp); each frame, the code changed only when the wish changes. Another build: a log line.
void quickTeleport(bool wanted);
// 3.0.13-beta.8: [interface] screen_shake 0 - Earthquake and the like no longer shake the camera
// (core/screen_shake_exe.hpp)
void stillScreen(bool wanted);
// 3.0.13-beta.8: [interface] skill_jokes 0 - Frost Joke and Scream say nothing in the chat (core/skill_jokes_exe.hpp)
void silentJokes(bool wanted);
// 3.0.13-beta.9: [interface] swear_filter 0 - the game no longer refuses words of manner.txt
// (core/swear_filter_exe.hpp)
void noSwearFilter(bool wanted);

}  // namespace uagfx::win
