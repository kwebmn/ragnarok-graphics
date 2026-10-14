#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::water {

// 0.19, modern water: how uaRO.exe draws a map's water (the decompiled ground renderer 0x4e59d0, the face list it
// submits to and the renderer's flush 0x40e4d0): one quad per ground cell under the water level, a strip of four
// pre-transformed vertices (FVF 0x1C4) blended with SRCALPHA / INVSRCALPHA without Z writes, after the map, the
// models and the characters. Every vertex is white with alpha 0x90, and the texture (one frame of
// water%d%02d.jpg) repeats every four cells: the corners' UVs are the cell's place modulo four times 0.25, a
// quarter apart. Lava (type 4) is opaque and stays the game's.
constexpr uint32_t kDiffuse = 0x90FFFFFF;
constexpr float kStep = 0.25f;  // the UV of one cell (0x748850, set at start)

// Whether four vertices (XYZRHW, diffuse at byte 16, UV at byte 24; `stride` bytes apart) drawn as a strip are one
// such quad.
bool isWaterQuad(const uint8_t* vertices, size_t stride, uint32_t vertexCount, uint32_t indexCount, bool strip);
// 0.31: a cell of lava (water type 4: mag_dun01, mag_dun02, tak_run in UaRO.grf) - the same quad, opaque, each corner
// in the map's light (one colour), blended alike.
bool isLavaQuad(const uint8_t* vertices, size_t stride, uint32_t vertexCount, uint32_t indexCount, bool strip);

}  // namespace uagfx::water
