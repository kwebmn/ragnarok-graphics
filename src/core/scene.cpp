#include "core/scene.hpp"

#include "core/d3d7.hpp"

namespace uagfx {

void SceneTracker::renderState(uint32_t state, uint32_t value) {
    if (state == d3d7::RS_ZENABLE && value == 0) ui_ = true;
}

}  // namespace uagfx
