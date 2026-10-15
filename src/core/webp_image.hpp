#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uagfx::webp {

// 0.42: the texture pack's pictures as WebP (libwebp, third_party). argb: width x height 0xAARRGGBB, straight alpha.
// quality 0-99 lossy, 100 lossless; the colour under transparent texels is kept (the enlarged pictures are filtered
// across their edges - another colour there would show as a fringe), the alpha losslessly.
std::vector<uint8_t> encode(const uint32_t* argb, int width, int height, int quality);
bool decode(const uint8_t* data, size_t size, std::vector<uint32_t>& argb, int& width, int& height);

}  // namespace uagfx::webp
