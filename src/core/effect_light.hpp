#pragma once
#include <cstdint>

namespace uagfx::effect_light {

// 0.26: how much a picture of an effect looks like fire, 0-1 - its pixels weighed by their alpha and brightness (an
// additive picture's black adds nothing), their average red well above blue, green between them; colour: that
// average, its brightest channel 1. The fire of skills lights around it (win/gpu/torches.cpp).
float fireOf(const uint32_t* argb, int width, int height, float colour[3]);
// 0.36: how much a picture of any other effect lights around it in its own colour, 0-1 - saturated colours (ice blue,
// poison green, holy gold) by how saturated they are, near-white ones only when bright (lightning); grey, dark and
// clear pictures none. colour as fireOf's.
float lightOf(const uint32_t* argb, int width, int height, float colour[3]);

}  // namespace uagfx::effect_light
