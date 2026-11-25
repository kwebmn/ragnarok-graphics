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
// 3.0.23-beta.11: its opacity in solids::kMaskSide cells (solids::opacityMask); false: its texels unknown
bool opacityOf(const void* surface, uint8_t* bits);
// 3.0.20-beta.74 (R-U5PP9T «вообще дом пропал полностью» in Prontera, R-64G2U9, R-7VX6Q9, the bridges R-HBPQ54 and
// R-3W4TYM - «Модели нашим рендером»): a texture's texels go to the renderer only with a draw of the GAME's that
// samples it (Device7::record), and the game draws a model's node itself in one frame alone - the frame of its
// capture, the faces on the screen and turned to the eye then (uaRO.exe FUN_004DE9F0); ours from the next frame on. A
// picture no face of which was drawn in that frame - the far side's, a house coming into the picture by its box's
// corner - never went up: the renderer's texture stayed as made (nothing in it), its faces lost to the alpha test.
// The game's one model in Prontera with pictures of its own alone (교역소) lost all four. So: a surface's waiting
// texels recorded now, as a draw of it would; true when its top level waited
bool sendTexelsOf(const void* surface);

}  // namespace uagfx::dx7
