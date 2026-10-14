#pragma once
#include <cstdint>

#include "core/map_axes.hpp"

namespace uagfx::fireflies {

// 0.28, the maps' fireflies drawn anew (win/gpu/torches.cpp): those the game shows (effect 45, its soft dot) and those
// the maps' makers placed that it does not (713, 714, 946 - "yellowfly").

// How bright a firefly is now, 0.18-1: a short flash every two or three seconds - up fast, a slower fade - and a
// faint glow between, each seed (0-1) its own rhythm.
float blink(double time, float seed);
// Where one of the map file's fireflies is now, from its place (world units; up is -y): drifting about it, within
// six units across, one to nine above.
map::Point wander(double time, float seed);
// A firefly's colour from the game's tint of its dot (ARGB), its brightest channel 1: the game's own, or a
// firefly's yellow-green where the tint is grey or black.
void colourOf(uint32_t diffuse, float colour[3]);

}  // namespace uagfx::fireflies
