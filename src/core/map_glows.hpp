#pragma once
#include <cstdint>

namespace uagfx::map_glows {

// 0.29, the maps' hidden lights (win/gpu/torches.cpp): effects the maps' makers placed that uaRO.exe never shows (it
// knows effect ids up to 629) - in UaRO.grf about a thousand glows in Manuk, Nidhoggur's nest and Splendide, two
// hundred pools of light in Splendide's fields, green lights, the towers' dust.
enum class Shape : uint8_t {
    Orb,   // a soft ball of light floating, breathing slowly, lighting around it
    Pool,  // light on the ground about its place, a faint glow over it
    Dust,  // motes drifting in the air about it, no light
};

struct Glow {
    Shape shape = Shape::Orb;
    float colour[3] = {1.0f, 1.0f, 1.0f};  // its brightest channel 1
    float size = 0.6f;    // world units: an orb's core, a dust cloud's radius
    float light = 0.0f;   // how much it lights around (0 none)
    float reach = 0.0f;   // world units
};

// 693, 694, 695, 701, 702 (glow1, glow2, glow4, glow11, glow12), 703 (circlelight), 710, 712 (map_greenlight, 2),
// 689 (dust); false for the rest.
bool glowOf(int id, Glow& glow);
// How bright an orb or a pool is now, 0.6-1: a slow breath, each seed (0-1) its own.
float pulse(double time, float seed);
// How far an orb has risen or sunk about its place now, about a world unit each way.
float bob(double time, float seed);

}  // namespace uagfx::map_glows
