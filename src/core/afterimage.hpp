#pragma once
#include <cstdint>

namespace uagfx::afterimage {

// 3.0.16-beta.13 (the owner's pick 15 of «20 идей»): the character's body tint, added over its sprite - Fury a warm
// red, Steel Body cool steel; 0 strength none. (.13's afterimages behind a fast hero gone in .14: «убери».)
struct Tint {
    uint8_t r = 0, g = 0, b = 0;
    float strength = 0;
};
Tint tintOf(bool fury, bool steelBody);

}  // namespace uagfx::afterimage
