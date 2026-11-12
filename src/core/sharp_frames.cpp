#include "core/sharp_frames.hpp"

#include <algorithm>
#include <cstring>

#include "core/full_colour.hpp"
#include "core/xbr.hpp"

namespace uagfx::sharpframes {

Copy makeCopy(const uint32_t* argb, int w, int h) {
    Copy c;
    if (!argb || w <= 0 || h <= 0 || w > kMostSide || h > kMostSide) return c;
    const bool small = std::max(w, h) <= kSmallSide;  // 3.0.18-beta.185: twice
    c.width = w * kScale * (small ? kScale : 1);
    c.height = h * kScale * (small ? kScale : 1);
    std::vector<uint32_t> top = xbr::scale4(argb, w, h);
    if (small) top = xbr::scale4(top.data(), w * kScale, h * kScale);
    c.levels.push_back(std::move(top));
    int lw = c.width, lh = c.height;
    while (lw > w || lh > h) {  // 2x, then the frame's own size
        std::vector<uint32_t> next;
        int nw = 0, nh = 0;
        full_colour::halve(c.levels.back(), lw, lh, next, nw, nh);  // weighted by alpha: no dark rims
        if (next.empty() || (nw == lw && nh == lh)) break;
        c.levels.push_back(std::move(next));
        lw = nw;
        lh = nh;
    }
    return c;
}

uint64_t keyOf(const uint32_t* argb, int w, int h) {
    uint64_t k = 0xcbf29ce484222325ull;
    const auto mix = [&k](uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            k ^= (v >> (i * 8)) & 0xFF;
            k *= 0x100000001b3ull;
        }
    };
    mix(uint32_t(w));
    mix(uint32_t(h));
    if (argb && w > 0 && h > 0)
        for (size_t i = 0, n = size_t(w) * size_t(h); i < n; ++i) mix(argb[i]);
    return k ? k : 1;
}

bool remap(uint8_t* vertices, uint32_t count, uint32_t stride, int texW, int texH, const Rect& frame) {
    if (!vertices || !count || stride < 32 || texW <= 0 || texH <= 0 || frame.empty()) return false;
    constexpr float kSlack = 0.5f;  // texels: the game's own rounding of its coordinates
    for (uint32_t i = 0; i < count; ++i) {
        float uv[2];
        std::memcpy(uv, vertices + size_t(i) * stride + 24, sizeof uv);
        const float x = uv[0] * float(texW), y = uv[1] * float(texH);
        if (!(x >= float(frame.left) - kSlack && x <= float(frame.right) + kSlack && y >= float(frame.top) - kSlack &&
              y <= float(frame.bottom) + kSlack))
            return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        float uv[2];
        uint8_t* at = vertices + size_t(i) * stride + 24;
        std::memcpy(uv, at, sizeof uv);
        uv[0] = (uv[0] * float(texW) - float(frame.left)) / float(frame.width());
        uv[1] = (uv[1] * float(texH) - float(frame.top)) / float(frame.height());
        std::memcpy(at, uv, sizeof uv);
    }
    return true;
}

}  // namespace uagfx::sharpframes
