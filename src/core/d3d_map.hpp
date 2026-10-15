#pragma once
#include <cstdint>

#include "core/states.hpp"

namespace uagfx::d3dmap {

// 0.46: Direct3D 7's states as the numbers Direct3D 11 and 12 share, for the Direct3D 11 renderer (the Direct3D 12
// one makes its own in pipeline.cpp; these follow it): filters (D3D1x_FILTER), address modes (WRAP 1 ... BORDER 4),
// comparison functions (NEVER 1 ... ALWAYS 8), blend factors (ZERO 1 ... SRCALPHASAT 11), cull modes (NONE 1, FRONT
// 2, BACK 3 - both count winding in window space, y down).
struct Sampler {
    uint32_t filter = 0;
    uint32_t addressU = 1, addressV = 1;
    uint32_t maxAnisotropy = 1;
    float border[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // R, G, B, A
    bool mips = false;  // every level, or the first alone
};
// A world texture the game filters linearly is filtered anisotropically when aniso is 2-16.
Sampler samplerOf(const StageSetup& s, int aniso);
uint32_t compareOf(uint8_t func);                // anything else: ALWAYS
uint32_t cullOf(uint8_t cull);                   // anything else: NONE
uint32_t blendOf(uint8_t factor, uint8_t none);  // a factor not set: `none` (ONE as a source, ZERO as a destination)

}  // namespace uagfx::d3dmap
