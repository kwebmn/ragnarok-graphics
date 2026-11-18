#pragma once
#include <algorithm>

#include "core/horizon.hpp"
#include "win/gpu/shaders.hpp"

namespace uagfx::gpu {

// 3.0.22-beta.11 (core/horizon): the horizon bake's numbers into the shared constants by core horizon::pack's table
// (shaders.cpp `hz...` names) - Direct3D 12's horizon.cpp and Direct3D 11's renderer11 alike
inline void horizonConstants(const float (&p)[horizon::kPackedFloats], Constants& c) {
    std::copy(p, p + 12, c.reproject);
    std::copy(p + 12, p + 20, c.hole);
    std::copy(p + 20, p + 24, c.fogColor);
    std::copy(p + 24, p + 28, c.sourceRect);
    std::copy(p + 28, p + 32, c.sourceClamp);
    std::copy(p + 32, p + 36, c.twin);
    std::copy(p + 36, p + 38, c.jitter);
    std::copy(p + 38, p + 44, c.projection);
}

}  // namespace uagfx::gpu
