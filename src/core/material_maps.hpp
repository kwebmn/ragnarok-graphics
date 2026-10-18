#pragma once
#include <cstdint>
#include <vector>

#include "core/texture_pack.hpp"

namespace uagfx::material {

// 2.3, uaRO Studio (T7): the material the driver makes of a texture's picture (shaders.cpp material_ps, 1.8) on the
// processor - its relief (the normal's x and y in r and g), its height (b) and its gloss (a, a window's pane 0.75-1)
// as RGBA bytes - at most 512 a side, with a texture's own scales. Kept in step with material_ps: the studio shows
// what the game will light the texture by.
constexpr int kMostSide = 512;  // Renderer::Impl::materialSide
void mapsOf(const uint32_t* argb, int w, int h, const pack::MaterialScales& scales, std::vector<uint32_t>& out,
            int& ow, int& oh);

}  // namespace uagfx::material
