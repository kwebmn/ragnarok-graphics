#pragma once
#include <cstddef>
#include <functional>

namespace uagfx::gpu {

// 3.0.20-beta.8: the own ground's threads (own_ground.cpp) - `count` jobs, the renderer's thread among them, the call
// back when all are done; the game's still models placed on them too (game_models.cpp); 3.0.20-beta.16 their leaves
// in both renderers (core gamemodels::Leaves). The renderer's thread alone calls it.
void parallelChunks(size_t count, const std::function<void(size_t)>& body);

}  // namespace uagfx::gpu
