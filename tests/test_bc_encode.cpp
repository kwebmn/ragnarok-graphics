#include <algorithm>
#include <cmath>
#include <vector>

#include "core/bc_encode.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A smooth picture like the AI's (gradients, a soft round shape), the alpha a soft edge when asked.
std::vector<uint32_t> picture(int w, int h, bool alpha) {
    std::vector<uint32_t> p(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double d = std::hypot(x - w / 2.0, y - h / 2.0) / (w / 2.0);
            const uint32_t r = uint32_t(40 + 200 * x / w), g = uint32_t(60 + 150 * y / h);
            const uint32_t b = uint32_t(200 * (1 - d / 2));
            const uint32_t a = alpha ? uint32_t(std::clamp(255.0 * (1.2 - d), 0.0, 255.0)) : 255;
            p[size_t(y) * size_t(w) + size_t(x)] = a << 24 | std::min(r, 255u) << 16 | std::min(g, 255u) << 8 |
                                                   std::min(b, 255u);
        }
    return p;
}

// The peak signal-to-noise ratio of the colours (and alpha), in dB.
double psnr(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, bool alpha) {
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i)
        for (int s = alpha ? 24 : 0; s <= (alpha ? 24 : 16); s += 8) {
            const double d = double(int(a[i] >> s & 255) - int(b[i] >> s & 255));
            sum += d * d;
        }
    const double mse = sum / double(a.size() * (alpha ? 1 : 3));
    return mse == 0 ? 99 : 10 * std::log10(255.0 * 255.0 / mse);
}

}  // namespace

