#include "core/ai/black_key.hpp"

#include "core/ai/texture_cache.hpp"

namespace uagfx::ai {

bool plainOfBlackKey(const uint32_t* argb, int width, int height, std::vector<uint32_t>& plain,
                     std::vector<uint8_t>& shown) {
    if (!argb || width <= 0 || height <= 0) return false;
    const size_t n = size_t(width) * size_t(height);
    size_t away = 0;
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = argb[i], a = p >> 24;
        if (p == 0xFF000000u || (a != 0 && a != 255)) return false;
        away += p == 0;
    }
    if (!away) return false;
    plain.assign(argb, argb + n);
    shown.assign(n, 1);
    for (size_t i = 0; i < n; ++i)
        if (!plain[i]) {  // black as A1R5G5B5 has it: the alpha bit, no colour
            plain[i] = 0xFF000000u;
            shown[i] = 0;
        }
    return true;
}

void keyAway(std::vector<uint32_t>& picture, int width, int height, const std::vector<uint8_t>& shown,
             int textureWidth, int textureHeight) {
    const int tw = textureWidth, th = textureHeight;
    if (width <= 0 || height <= 0 || tw <= 0 || th <= 0 || picture.size() < size_t(width) * size_t(height) ||
        shown.size() < size_t(tw) * size_t(th))
        return;
    std::vector<uint8_t> alpha;
    if (width % tw == 0 && height % th == 0 && width / tw == height / th) {
        std::vector<float> a(size_t(tw) * size_t(th));
        for (size_t i = 0; i < a.size(); ++i) a[i] = shown[i] ? 1.0f : 0.0f;
        enlargeAlpha(a.data(), tw, th, width / tw, alpha);
    } else {  // no whole factor: the nearest texel's
        alpha.resize(size_t(width) * size_t(height));
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                alpha[size_t(y) * size_t(width) + size_t(x)] =
                    shown[size_t(y * th / height) * size_t(tw) + size_t(x * tw / width)] ? 255 : 0;
    }
    for (size_t i = 0; i < alpha.size(); ++i)
        if (!alpha[i]) picture[i] &= 0x00FFFFFFu;  // its colour stays: the filtering's at the key's edge
}

}  // namespace uagfx::ai
