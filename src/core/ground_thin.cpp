#include "core/ground_thin.hpp"

namespace uagfx::ground_thin {

bool workedOut(float viewDepth, uint32_t key, uint32_t frame) {
    if (!(viewDepth > kNearDepth)) return true;
    const uint32_t turn = ((key >> 7) * 2654435761u) >> 29;  // kEvery 8: three bits of the block's number, mixed
    return turn == (frame & (kEvery - 1));
}

}  // namespace uagfx::ground_thin
