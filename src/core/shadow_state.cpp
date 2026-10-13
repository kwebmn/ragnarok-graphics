#include "core/shadow_state.hpp"

namespace uagfx {

ShadowState::Touch ShadowState::touch(bool write) {
    Touch t{stale_, write};
    stale_ = false;
    if (write) dirty_ = true;
    return t;
}

bool ShadowState::unlock() {
    locked_ = false;
    return dirty_;  // after the upload the copy and the target agree: stale_ stays false
}

}  // namespace uagfx
