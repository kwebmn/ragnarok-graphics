#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::mounticon {

// 3.0.18-beta.116 (the owner chose «2 Значок між стрілками»): «Спорядження»'s button for the cart, the falcon, the Peco
// as a sign alone - a bracket (a door) on the left, an arrow out of it to the right, drawn smooth in `rgb` over
// premultiplied 0xAARRGGBB pixels (w x h, rows top down)
void paint(std::vector<uint32_t>& px, int w, int h, uint32_t rgb);

}  // namespace uagfx::mounticon
