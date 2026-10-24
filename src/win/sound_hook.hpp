#pragma once
#include <string>

namespace uagfx::win {

// 0.91: the game's sound (Miles Sound System) through uaRO.exe's imports of mss32.dll, taken over when ddraw.dll is
// loaded (before the game opens its sound): the digital driver opened at [sound] rate, the most voices the game's
// tables hold, every sample's and stream's volume times the limiter's and the background's gain
// (core/sound_mix.hpp). Nothing when the exe imports no mss32.dll.
void installSoundHooks();
// Each frame: whether the game is in front - the volumes follow it; the first one logs what was done. 3.0.3-beta.3:
// `music` false - the music out (another game window is the main one), back when true.
// 3.0.3-beta.9: sounds - this game's sounds heard (off: another game window is the main one, [windows] others_sounds)
void soundFrame(bool foreground, bool music = true, bool sounds = true);
// 3.0.4-beta.1: [sound] output changed in a running game - the music and the sounds move to it at once
void soundOutput(const std::string& id);

}  // namespace uagfx::win
