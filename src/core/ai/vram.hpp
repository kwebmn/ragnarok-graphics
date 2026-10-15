#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::ai {

// 0.44: the AI copies of the game's textures in video memory - 16 times a texture's bytes and more - keep to a budget.
// Over it, the copies of the textures not drawn for longest are put away (a texture drawn again gets its copy back
// from the texture pack or upscaled.grf, a read away).
struct Copy {
    uint32_t id = 0;        // the game texture's
    uint64_t bytes = 0;     // its copy's, all its mip levels
    uint64_t lastUse = 0;   // the frame it was last drawn in
};
// The copies to put away, oldest first: until the total is a tenth under the budget, of the ones not drawn in the last
// minAge frames. None when the total is within the budget or there is no budget.
std::vector<uint32_t> putAway(const std::vector<Copy>& copies, uint64_t total, uint64_t budget, uint64_t now,
                              uint64_t minAge);
// The copies' budget: 40 % of the video memory the system gives the game (DXGI's budget), at least 256 MB; 1 GB when
// that is not known.
uint64_t copiesBudget(uint64_t systemBudget);

}  // namespace uagfx::ai
