#pragma once

namespace uagfx::win {

// 0.91: the game's sound (Miles Sound System) through uaRO.exe's imports of mss32.dll, taken over when ddraw.dll is
// loaded (before the game opens its sound): the digital driver opened at [sound] rate, the most voices the game's
// tables hold, every sample's and stream's volume times the limiter's and the background's gain
// (core/sound_mix.hpp). Nothing when the exe imports no mss32.dll.
void installSoundHooks();
// Each frame: whether the game is in front - the volumes follow it; the first one logs what was done.
void soundFrame(bool foreground);

}  // namespace uagfx::win
