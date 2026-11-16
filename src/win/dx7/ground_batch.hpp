#pragma once
#include <cstdint>

namespace uagfx::dx7 {

// 3.0.19-beta.50 (core/quad_batch; Device7): the live device's gathered ground cells recorded now (the frame about
// to go, a surface about to change), and [quality] batch_ground - free of Direct3D 7's own headers
void flushGroundBatch();
void setGroundBatch(bool on);
// 3.0.20-beta.3 (stage 2a of «Модели нашим рендером»): the renderer's texture of a surface the game holds (its
// CTexture +0xC); 0 for none of ours
uint32_t gpuTextureOf(const void* surface);
// 3.0.20-beta.70 (core/camera_solids): how much of a surface's picture is holes (percent of its texels keyed out), as
// the game last wrote it; -1 for none of ours
int holesOf(const void* surface);

}  // namespace uagfx::dx7
