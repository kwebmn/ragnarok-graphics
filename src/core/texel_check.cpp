#include "core/texel_check.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace uagfx::texelcheck {
namespace {

int channel(uint32_t v, int shift) { return int((v >> shift) & 0xFFu); }

}  // namespace

Compare compare(const uint32_t* game, const uint32_t* card, size_t n) {
    Compare c;
    c.texels = n;
    if (!game || !card) return c;
    uint64_t sum[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < n; ++i) {
        const uint32_t g = game[i], k = card[i];
        const int ga = channel(g, 24), ka = channel(k, 24);
        bool apart = std::abs(ga - ka) > kNear;
        if (!apart && ga >= kNear && ka >= kNear)  // see-through on both sides: whatever colour lies under it
            for (int shift = 0; shift < 24 && !apart; shift += 8)
                apart = std::abs(channel(g, shift) - channel(k, shift)) > kNear;
        if (apart) ++c.differ;
        if (ka >= 128 && ga < kNear) {
            ++c.cardOnly;
            for (int j = 0; j < 4; ++j) sum[j] += uint64_t(channel(k, j * 8));
        }
        if (ga >= 128 && ka < kNear) ++c.gameOnly;
    }
    if (c.cardOnly)
        for (int j = 0; j < 4; ++j) c.cardOnlyMean |= uint32_t((sum[j] + c.cardOnly / 2) / c.cardOnly) << (j * 8);
    return c;
}

Rect rectOf(const std::vector<uint8_t>& bytes, size_t offset, size_t stride, uint32_t count, int width, int height) {
    if (!count || stride < 32 || width <= 0 || height <= 0 || offset + size_t(count) * stride > bytes.size())
        return Rect{};
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        float uv[2];
        std::memcpy(uv, bytes.data() + offset + size_t(i) * stride + 24, sizeof uv);
        if (!std::isfinite(uv[0]) || !std::isfinite(uv[1])) return Rect{};
        u0 = i ? std::min(u0, uv[0]) : uv[0];
        v0 = i ? std::min(v0, uv[1]) : uv[1];
        u1 = i ? std::max(u1, uv[0]) : uv[0];
        v1 = i ? std::max(v1, uv[1]) : uv[1];
    }
    const float w = float(width), h = float(height);
    const auto inside = [](float x, float most) { return int(std::clamp(x, 0.0f, most)); };
    const Rect r{inside(std::floor(u0 * w), w), inside(std::floor(v0 * h), h), inside(std::ceil(u1 * w), w),
                 inside(std::ceil(v1 * h), h)};
    return r.empty() ? Rect{} : r;
}

std::string bmpOf(const uint32_t* argb, int width, int height) {
    if (!argb || width <= 0 || height <= 0) return std::string();
    const size_t texels = size_t(width) * size_t(height);
    std::string b(54 + texels * 4, '\0');
    const auto put = [&b](size_t at, uint32_t v, int bytes) {
        for (int i = 0; i < bytes; ++i) b[at + size_t(i)] = char((v >> (8 * i)) & 0xFFu);
    };
    b[0] = 'B';
    b[1] = 'M';
    put(2, uint32_t(b.size()), 4);
    put(10, 54, 4);                         // the texels' place
    put(14, 40, 4);                         // BITMAPINFOHEADER
    put(18, uint32_t(width), 4);
    put(22, uint32_t(-int64_t(height)), 4); // negative: the rows top-down
    put(26, 1, 2);                          // planes
    put(28, 32, 2);                         // bits a texel
    put(34, uint32_t(texels * 4), 4);       // BI_RGB (0 at 30), the texels' bytes
    put(38, 2835, 4);                       // 72 dots an inch
    put(42, 2835, 4);
    for (size_t i = 0; i < texels; ++i) put(54 + i * 4, argb[i], 4);  // an ARGB word's bytes: B G R A
    return b;
}

}  // namespace uagfx::texelcheck
