#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::bc {

// 0.57: the AI textures compressed in video memory (Direct3D 11, [ai] compress) - BC1 for the opaque ones (8 bytes a
// 4x4 block: an eighth of B8G8R8A8), BC3 for the rest (16: a quarter). A fast range fit: the pictures are smooth and
// a weak card's memory is worth more than the last bit of their colour. Pixels 0xAARRGGBB; any size, the edge blocks
// filled with the edge's texels.
bool opaque(const uint32_t* argb, int width, int height);
std::vector<uint8_t> encodeBC1(const uint32_t* argb, int width, int height);
std::vector<uint8_t> encodeBC3(const uint32_t* argb, int width, int height);
// For the tests: back to 0xAARRGGBB.
std::vector<uint32_t> decodeBC1(const uint8_t* blocks, int width, int height);
std::vector<uint32_t> decodeBC3(const uint8_t* blocks, int width, int height);
inline int blocksAcross(int side) { return (side + 3) / 4; }

}  // namespace uagfx::bc
