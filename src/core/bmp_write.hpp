#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::bmp {

// 0.49.1: a picture as a 32-bit BMP (top-down, its alpha kept) - what the pack had no picture of, saved for the
// developers (uaro-graphics\missing\). Empty for no picture.
std::vector<uint8_t> write32(const uint32_t* argb, int width, int height);

}  // namespace uagfx::bmp
