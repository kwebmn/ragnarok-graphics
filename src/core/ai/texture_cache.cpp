#include "core/ai/texture_cache.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace uagfx::ai {
namespace {

void put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i)));
}
uint32_t get32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

}  // namespace

TextureKey textureKey(const uint32_t* argb, int width, int height) {
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            h ^= (v >> (8 * i)) & 0xFF;
            h *= 1099511628211ull;
        }
    };
    mix(uint32_t(width));
    mix(uint32_t(height));
    for (size_t i = 0, n = size_t(width) * size_t(height); i < n; ++i) mix(argb[i]);
    return TextureKey{h, width, height};
}

std::string entryName(const std::string& model, const TextureKey& k) {
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(k.hash));
    return "ai\\" + model + "\\" + std::to_string(k.width) + "x" + std::to_string(k.height) + "\\" + hex + ".tex";
}

std::vector<uint8_t> encodeTexture(const uint32_t* argb, int width, int height) {
    std::vector<uint8_t> b;
    b.reserve(16 + size_t(width) * size_t(height) * 4);
    for (const char c : {'U', 'A', 'T', 'X'}) b.push_back(uint8_t(c));  // (insert of a list: a false GCC warning)
    put32(b, 1);
    put32(b, uint32_t(width));
    put32(b, uint32_t(height));
    for (size_t i = 0, n = size_t(width) * size_t(height); i < n; ++i) put32(b, argb[i]);
    return b;
}

bool decodeTexture(const std::vector<uint8_t>& blob, std::vector<uint32_t>& argb, int& width, int& height) {
    if (blob.size() < 16 || std::memcmp(blob.data(), "UATX", 4) != 0 || get32(blob.data() + 4) != 1) return false;
    const uint32_t w = get32(blob.data() + 8), h = get32(blob.data() + 12);
    if (w == 0 || h == 0 || w > 16384 || h > 16384 || blob.size() != 16 + size_t(w) * h * 4) return false;
    width = int(w);
    height = int(h);
    argb.resize(size_t(w) * h);
    for (size_t i = 0; i < argb.size(); ++i) argb[i] = get32(blob.data() + 16 + i * 4);
    return true;
}

bool hasTransparency(const uint32_t* argb, int width, int height) {
    for (size_t i = 0, n = size_t(width) * size_t(height); i < n; ++i)
        if ((argb[i] >> 24) < 128) return true;
    return false;
}

void prepareInput(const uint32_t* argb, int width, int height, std::vector<float>& planes) {
    const size_t n = size_t(width) * size_t(height);
    planes.assign(n * 4, 0.0f);
    std::vector<uint8_t> known(n);
    std::vector<float> rgb(n * 3);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = argb[i];
        known[i] = (p >> 24) >= 128;
        rgb[i * 3] = float((p >> 16) & 0xFF) / 255.0f;
        rgb[i * 3 + 1] = float((p >> 8) & 0xFF) / 255.0f;
        rgb[i * 3 + 2] = float(p & 0xFF) / 255.0f;
        planes[n * 3 + i] = known[i] ? 1.0f : 0.0f;
    }
    // Hidden texels take the average of their known neighbours, ring by ring inwards.
    for (bool changed = true; changed;) {
        changed = false;
        std::vector<size_t> fill;
        std::vector<float> value;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const size_t i = size_t(y) * size_t(width) + size_t(x);
                if (known[i]) continue;
                float s[3] = {0, 0, 0};
                int k = 0;
                const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    const int xx = x + dx[d], yy = y + dy[d];
                    if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
                    const size_t j = size_t(yy) * size_t(width) + size_t(xx);
                    if (!known[j]) continue;
                    for (int c = 0; c < 3; ++c) s[c] += rgb[j * 3 + c];
                    ++k;
                }
                if (!k) continue;
                fill.push_back(i);
                for (int c = 0; c < 3; ++c) value.push_back(s[c] / float(k));
            }
        for (size_t f = 0; f < fill.size(); ++f) {
            for (int c = 0; c < 3; ++c) rgb[fill[f] * 3 + c] = value[f * 3 + c];
            known[fill[f]] = 1;
            changed = true;
        }
    }
    for (size_t i = 0; i < n; ++i)
        for (int c = 0; c < 3; ++c) planes[size_t(c) * n + i] = rgb[i * 3 + c];
}

void enlargeAlpha(const float* alpha, int width, int height, int scale, std::vector<uint8_t>& out) {
    const int ow = width * scale, oh = height * scale;
    out.assign(size_t(ow) * size_t(oh), 0);
    for (int y = 0; y < oh; ++y)
        for (int x = 0; x < ow; ++x) {
            // The output pixel's centre in input texels, between the input's texel centres, clamped at the edges.
            const float fx = std::clamp((float(x) + 0.5f) / float(scale) - 0.5f, 0.0f, float(width - 1));
            const float fy = std::clamp((float(y) + 0.5f) / float(scale) - 0.5f, 0.0f, float(height - 1));
            const int x0 = int(fx), y0 = int(fy);
            const int x1 = std::min(x0 + 1, width - 1), y1 = std::min(y0 + 1, height - 1);
            const float tx = fx - float(x0), ty = fy - float(y0);
            const auto at = [&](int xx, int yy) { return alpha[size_t(yy) * size_t(width) + size_t(xx)]; };
            const float v = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) +
                            (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
            out[size_t(y) * size_t(ow) + size_t(x)] = v >= 0.5f ? 255 : 0;
        }
}

}  // namespace uagfx::ai
