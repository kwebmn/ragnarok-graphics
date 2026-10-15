#include "core/lightning.hpp"

#include <cmath>
#include <cstdint>

namespace uagfx::lightning {

float flash(double seconds) {
    if (seconds < 0) return 0.0f;
    const double window = 9.0;
    const auto w = uint32_t(seconds / window);
    uint32_t h = w * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    if (h % 100 >= 45) return 0.0f;  // a quiet window
    const double at = double(w) * window + double((h >> 8) % 800) / 100.0;  // 0-8 s into it
    const double dt = seconds - at;
    if (dt < 0.0 || dt > 0.9) return 0.0f;
    if (dt < 0.07) return 1.0f;                                   // the first stroke
    if (dt < 0.16) return 0.25f;
    if (dt < 0.24) return 0.8f;                                   // the second
    return float(0.8 * std::exp(-(dt - 0.24) * 7.0));            // and the glow fading
}

}  // namespace uagfx::lightning
