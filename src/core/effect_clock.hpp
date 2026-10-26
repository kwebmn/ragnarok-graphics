#pragma once

#include <cstdint>

namespace uagfx::effect_clock {

// 3.0.6-beta.2 (a player: «свернув окно, зайшов в браузер, зайшов в гру і все чорно» - the world black, a portal
// brought it back): Direct3D 12's effects' clock went round every hour for its shaders' floats, and what took ages
// from it on the processor - the new map's fade, the steps, the torches kept, the creatures - had them below nothing
// when it did: the world stayed black on a map entered later in the hour than the clock was. The seconds here never
// go round; the shaders take them an hour round.
class Clock {
public:
    double seconds(int64_t ticks, int64_t frequency);  // QueryPerformanceCounter's ticks; the first reading is 0

private:
    int64_t first_ = 0;
    bool started_ = false;
};

float forShaders(double seconds);  // an hour round: the floats keep their precision
// 0.83: the world coming in from black after a new map, 0-1 (1 shown), 0.7 s from `start`; below 0: none
float mapFade(double now, double start);

}  // namespace uagfx::effect_clock
