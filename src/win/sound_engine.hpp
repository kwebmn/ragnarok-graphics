#pragma once
#include <string>

namespace uagfx::win::audio {

// 1.1: the game's sounds at a place played by the driver's own engine (XAudio2 2.9, Windows 10 and later) instead of
// Miles: placed around the player for any speakers (stereo, 5.1, 7.1), quieter and muffled with the distance, with
// the place's reverberation ([sound] engine; core/sound_space.hpp, core/sound_place.hpp). Miles keeps the music and
// whatever this engine cannot play.

// A sound of the game's at (x, y, z) from the player: its file image (RIFF WAVE), its reach and full-volume distance,
// the game's volume factor. False: not taken (no engine, not this format) - the game plays it through Miles.
bool play(const void* image, float x, float y, float z, int reach, int inner, float factor);
// Each frame: the game in front or not, the map (its reverberation); the finished voices freed. The first one logs.
void frame(bool foreground, const std::string& map);
// Its settings: on, the reverberation's strength (0-100), the background's volume (%), the crowd's limiter, (1.2)
// Windows' spatial sound when it is on (headphones).
void configure(bool on, int reverb, int background, bool limiter, bool headphones);

}  // namespace uagfx::win::audio
