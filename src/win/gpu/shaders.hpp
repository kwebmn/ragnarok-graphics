#pragma once
#include <cstdint>

namespace uagfx::gpu {

// HLSL for shader model 5.0: draw_vs/draw_ps emulate Direct3D 7's fixed pipeline for pre-transformed vertices,
// present_vs/present_ps scale the client's target into the window.
const char* shaderSource();

// The shaders' cbuffer Params as the renderers fill it: Direct3D 12's root constants, (0.46) Direct3D 11's constant
// buffer (its size rounded up to 16 bytes).
struct Constants {
    float targetSize[2];   // 0-1: the target's size in pixels
    // 2-3: colorOp | colorArg1 << 8 | colorArg2 << 16 | alphaOp << 24,
    //      alphaArg1 | alphaArg2 << 8 | texCoord << 16 | keyed << 24
    uint32_t stage0[2];
    uint32_t stage1[2];    // 4-5
    uint32_t alphaTest;    // 6: func | ref << 8 | enabled << 16 | coverage << 17 | opaque in the layer << 18
    uint32_t fogEnable;    // 7
    float fogColor[4];     // 8-11
    uint32_t colorKey;     // 12
    uint32_t uiMode;       // 13: uiFilterMode() - 0 none, 1 sharp, 2 smooth (interface draws at k > 1); 3 a twin
    float scale;           // 14: k, game pixels -> target pixels (stage 3)
    float footprint;       // 15: blit only - source texels per destination pixel
    float sourceRect[4];   // 16-19: blit only, normalised u0 v0 u1 v1
    float uiPixels[4];     // 20-23: uiMode only - the quad's game pixels: first x, first y, last x, last y
    float sourceClamp[4];  // 24-27: blit only - the outermost texel centres of the source rectangle, normalised
    float sharpen;         // 28: blit only - CAS sharpness 0..1 of the picture on screen, below 0 = none
    float twin[4];         // 29-32: uiMode 3 only (stage 6) - the twin texel origin O (x, y), the twin texture's size
    float jitter[2];       // 33-34: part 3 (DLSS) - the world's geometry moved this frame, target pixels
    float reproject[12];   // 35-46: motion only - this view -> the previous one (4 x 3)
    float projection[6];   // 47-52: motion only - f0, f1, cx, cy, near, far (core/game_camera.hpp)
};
constexpr uint32_t kConstantCount = sizeof(Constants) / 4;
static_assert(kConstantCount == 53, "Constants must stay 53 DWORDs: the cbuffer in shaders.cpp");

}  // namespace uagfx::gpu
