#pragma once
#include <cstdint>
#include <vector>

#include "core/image.hpp"

// 3.0.20-beta.82 (R-9VCE45, core/texel_check.hpp): the game's own texels of a texture by its GPU texture, for the
// renderer's check of its copy (Renderer::setGameTexels) - without ddraw.h, which the DXGI headers clash with
namespace uagfx::dx7 {

// Level 0's texels of r as an upload sends them (Surface7::gameTexels); false: no such texture, or r not inside it
bool gameTexels(void* context, uint32_t gpuTexture, const Rect& r, std::vector<uint32_t>& out);

}  // namespace uagfx::dx7
