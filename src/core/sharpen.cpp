#include "core/sharpen.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx {

double casSharpen(double c, double n, double s, double w, double e, double sharpness) {
    const double mn = std::min({c, n, s, w, e});
    const double mx = std::max({c, n, s, w, e});
    // Less sharpening near black and white, where it would clip; none in flat areas (mn == mx: the sum is 4c).
    const double amp = std::sqrt(std::clamp(std::min(mn, 1.0 - mx) / std::max(mx, 1e-5), 0.0, 1.0));
    const double k = amp * (-1.0 / (8.0 + (5.0 - 8.0) * sharpness));  // -1/8 .. -1/5: the weights stay positive
    return std::clamp((c + k * (n + s + w + e)) / (1.0 + 4.0 * k), mn, mx);
}

}  // namespace uagfx
