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

TEST(bc_encode_keeps_a_smooth_picture) {
    // 0.57: BC1 for an opaque AI texture, BC3 for one with alpha - close to the picture (30 dB and more).
    const std::vector<uint32_t> p = picture(64, 32, false);
    CHECK(bc::opaque(p.data(), 64, 32));
    const std::vector<uint8_t> b1 = bc::encodeBC1(p.data(), 64, 32);
    CHECK_EQ(b1.size(), size_t(16 * 8 * 8));
    CHECK(psnr(p, bc::decodeBC1(b1.data(), 64, 32), false) > 30);
    const std::vector<uint32_t> q = picture(32, 32, true);
    CHECK(!bc::opaque(q.data(), 32, 32));
    const std::vector<uint8_t> b3 = bc::encodeBC3(q.data(), 32, 32);
    CHECK_EQ(b3.size(), size_t(8 * 8 * 16));
    const std::vector<uint32_t> back = bc::decodeBC3(b3.data(), 32, 32);
    CHECK(psnr(q, back, false) > 30);
    CHECK(psnr(q, back, true) > 35);
}

TEST(bc_encode_odd_sizes_and_flat_blocks) {
    // The mip levels' last ones (2x2, 1x1) are a block each; a flat block is exact.
    const std::vector<uint32_t> one = {0xFF336699u};
    const std::vector<uint8_t> b = bc::encodeBC1(one.data(), 1, 1);
    CHECK_EQ(b.size(), size_t(8));
    const uint32_t back = bc::decodeBC1(b.data(), 1, 1)[0];
    CHECK(std::abs(int(back >> 16 & 255) - 0x33) <= 4 && std::abs(int(back >> 8 & 255) - 0x66) <= 2 &&
          std::abs(int(back & 255) - 0x99) <= 4);
    const std::vector<uint32_t> p = picture(6, 5, true);
    CHECK_EQ(bc::encodeBC3(p.data(), 6, 5).size(), size_t(2 * 2 * 16));
    const std::vector<uint32_t> clear(16, 0x00000000u);
    CHECK((bc::decodeBC3(bc::encodeBC3(clear.data(), 4, 4).data(), 4, 4)[0] >> 24) == 0);
}
