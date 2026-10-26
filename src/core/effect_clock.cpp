#include "core/effect_clock.hpp"

namespace uagfx::effect_clock {

double Clock::seconds(int64_t ticks, int64_t frequency) {
    if (!started_) {
        started_ = true;
        first_ = ticks;
    }
    return frequency > 0 ? double(ticks - first_) / double(frequency) : 0.0;
}

float forShaders(double seconds) { return float(seconds - 3600.0 * double(int64_t(seconds / 3600.0))); }

float mapFade(double now, double start) {
    if (start < 0.0) return 1.0f;
    const float t = float((now - start) / 0.7);
    return t >= 1.0f ? 1.0f : t <= 0.0f ? 0.0f : t * t * (3.0f - 2.0f * t);
}

}  // namespace uagfx::effect_clock
