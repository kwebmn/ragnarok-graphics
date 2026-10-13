#pragma once

namespace uagfx::gpu {

// HLSL for shader model 5.0: draw_vs/draw_ps emulate Direct3D 7's fixed pipeline for pre-transformed vertices,
// present_vs/present_ps scale the client's target into the window.
const char* shaderSource();

}  // namespace uagfx::gpu
