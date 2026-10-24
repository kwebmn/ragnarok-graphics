#pragma once
#include <cstdint>
#include <string>

#include "core/wav.hpp"

namespace uagfx::win::spatial {

// 1.2: the engine's sounds as objects of Windows' spatial sound (Windows Sonic for Headphones, Dolby Atmos, DTS
// Headphone:X - when one is turned on in Windows): each sound placed around the listener in three dimensions, for
// headphones made binaural by Windows itself. Its own thread (COM, the stream's event); the engine's reverberation
// stays in XAudio2. Nothing when Windows has no spatial sound on.

// Its start on the engine's start: true when Windows takes the objects. 3.0.4-beta.1: on the engine's output device
// (Windows' endpoint id, "" its default); stopped as the engine is made anew.
bool start(const std::string& device);
void stop();
bool active();
// A sound: its samples (copied), from `azimuth` degrees (+ right) at `meters`, its gain and muffle. False when there is
// no object free for it (the engine plays it as before).
bool add(const wav::Sound& s, float azimuth, float meters, float gain, float cutoffHz);
void setMaster(float gain);  // the background's volume
const char* name();          // for the log: what renders it

}  // namespace uagfx::win::spatial
