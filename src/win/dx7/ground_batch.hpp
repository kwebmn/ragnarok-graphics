#pragma once

namespace uagfx::dx7 {

// 3.0.19-beta.50 (core/quad_batch; Device7): the live device's gathered ground cells recorded now (the frame about
// to go, a surface about to change), and [quality] batch_ground - free of Direct3D 7's own headers
void flushGroundBatch();
void setGroundBatch(bool on);

}  // namespace uagfx::dx7
