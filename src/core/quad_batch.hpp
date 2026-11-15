#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uagfx::quadbatch {

// 3.0.19-beta.50 (the owner: «3 можем сделать?» - the game's faces handed over in batches): uaRO.exe draws each ground
// cell by a DrawPrimitive of its own (FUN_0040BF60: a 4-vertex triangle strip, FVF 0x2C4, the list sorted by its
// textures - FUN_0040E4D0), thousands a frame, each through the driver's whole draw. Those in a row with the same
// textures and states made one indexed triangle list: a strip's (0 1 2) and (2 1 3) - the card's own turn of each
constexpr uint32_t kMostQuads = 16383;  // 16-bit indices: 4 vertices a quad
void append(std::vector<uint8_t>& vertices, std::vector<uint16_t>& indices, const uint8_t* quad, size_t stride);
// The renderers' test of a draw made so: quads in a row, 6 indices each
bool isBatch(uint32_t vertexCount, uint32_t indexCount);

}  // namespace uagfx::quadbatch
