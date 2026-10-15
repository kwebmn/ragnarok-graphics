#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uagfx::jpeg {

// 0.49: a .jpg of the game (the loading screens, the water) as ARGB, every pixel opaque - stb_image's decoder: the
// game's own (ijl15.dll) may come out a level or two apart, so these pictures are found by likeness (near_picture.hpp).
bool decode(const uint8_t* data, size_t size, std::vector<uint32_t>& argb, int& width, int& height);

}  // namespace uagfx::jpeg
