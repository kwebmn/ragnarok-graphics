#include "core/ui_glide.hpp"

#include <cmath>

namespace uagfx::uiglide {

float Glide::step(float target, uint64_t nowMs, bool on) {
    if (!on || !started) {
        started = true;
        at = nowMs;
        return shown = target;
    }
    const float dt = float(nowMs > at ? nowMs - at : 0);
    at = nowMs;
    shown += (target - shown) * (1.0f - std::exp(-dt / 50.0f));  // 95 % in 150 ms
    if (std::fabs(target - shown) < 0.05f) shown = target;
    return shown;
}

}  // namespace uagfx::uiglide
