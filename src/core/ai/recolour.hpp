#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::ai {

// 2.5.23: a sprite's frame the game drew in a dye (another palette: the clothes', the hair's) from the pack's
// picture of the frame in its own colours. Each texel's change of colour (dyed - own, both w x h, the game's colours
// of the same frame) is blended between the texels' places as the picture is larger (W x H) and added to it: the
// network's shading and lines stay, the colours are the dye's. Texels transparent in own (alpha under 128) give
// nothing; the picture's alpha stays.
void recolour(std::vector<uint32_t>& pic, int W, int H, const uint32_t* own, const uint32_t* dyed, int w, int h);

}  // namespace uagfx::ai
