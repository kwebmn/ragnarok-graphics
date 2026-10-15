#include "core/ai/texture_cache.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace uagfx::ai {
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

std::string keyName(const TextureKey& k) {
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(k.hash));
    return std::to_string(k.width) + "x" + std::to_string(k.height) + "\\" + hex;
}

std::string mapName(const char* raw, size_t size) {
    std::string out;
    for (size_t i = 0; i < size && raw[i]; ++i) {
        const char c = char(std::tolower(static_cast<unsigned char>(raw[i])));
        if (c == '.') break;
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '@' && c != '-') return "";
        out += c;
    }
    return out.size() <= 40 ? out : "";
}

void halve(std::vector<uint32_t>& argb, int& width, int& height) {
    const int w = std::max(1, width / 2), h = std::max(1, height / 2);
    if (argb.size() < size_t(width) * size_t(height) || width < 2 || height < 2) return;
    std::vector<uint32_t> out(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint32_t a = 0, c[3] = {0, 0, 0};
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const uint32_t p = argb[size_t(2 * y + dy) * size_t(width) + size_t(2 * x + dx)];
                    const uint32_t pa = p >> 24;
                    a += pa;
                    for (int k = 0; k < 3; ++k) c[k] += ((p >> (8 * k)) & 255) * pa;
                }
            uint32_t v = (a + 2) / 4 << 24;
            if (a)
                for (int k = 0; k < 3; ++k) v |= std::min<uint32_t>((c[k] + a / 2) / a, 255) << (8 * k);
            out[size_t(y) * size_t(w) + size_t(x)] = a ? v : 0;
        }
    argb = std::move(out);
    width = w;
    height = h;
}

bool hasTransparency(const uint32_t* argb, int width, int height) {
    for (size_t i = 0, n = size_t(width) * size_t(height); i < n; ++i)
        if ((argb[i] >> 24) < 128) return true;
    return false;
}

bool softAlpha(const uint32_t* argb, int width, int height) {
    if (!argb || width <= 0 || height <= 0) return false;
    const size_t n = size_t(width) * size_t(height);
    size_t partial = 0;
    for (size_t i = 0; i < n; ++i) {
        const uint32_t a = argb[i] >> 24;
        partial += a >= 16 && a < 240;
    }
    return partial * 20 > n;
}

bool softPicture(const uint32_t* argb, int width, int height) {
    if (!argb || width < 3 || height < 3) return false;  // too small to tell: as before
    // 0.22.7: three rows of the premultiplied colour at a time, times 255 (no division), and done as soon as more
    // than 2 % of the texels have an edge or a grain - most textures show that in their first rows. The renderer
    // asks this of every texture as the game loads a map (0.22.2-0.22.6 took 3 ms a 512 x 512 one).
    const size_t w = size_t(width);
    const size_t interior = (w - 2) * size_t(height - 2);
    const size_t limit = interior / 50;
    std::vector<int> ring(w * 9);  // three rows of R, G, B
    const auto fill = [&](int y) {
        int* r = ring.data() + size_t(y % 3) * w * 3;
        const uint32_t* p = argb + size_t(y) * w;
        for (size_t x = 0; x < w; ++x) {
            const int a = int(p[x] >> 24);
            r[3 * x] = int(p[x] & 255) * a;
            r[3 * x + 1] = int((p[x] >> 8) & 255) * a;
            r[3 * x + 2] = int((p[x] >> 16) & 255) * a;
        }
    };
    fill(0);
    fill(1);
    size_t detailed = 0;
    for (int y = 1; y + 1 < height; ++y) {
        fill(y + 1);
        const int* up = ring.data() + size_t((y - 1) % 3) * w * 3;
        const int* mid = ring.data() + size_t(y % 3) * w * 3;
        const int* down = ring.data() + size_t((y + 1) % 3) * w * 3;
        for (size_t x = 1; x + 1 < w; ++x) {
            int most = 0;
            for (size_t k = 0; k < 3; ++k) {
                const size_t i = 3 * x + k;
                const int lap = 4 * mid[i] - mid[i - 3] - mid[i + 3] - up[i] - down[i];
                most = std::max(most, lap < 0 ? -lap : lap);
            }
            if (most > 24 * 255 && ++detailed > limit) return false;
        }
    }
    return true;
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
    // Hidden texels take the average of their known neighbours, ring by ring inwards: each ring is the hidden texels
    // next to a known one, all of it from the texels known before it (a walk outwards from them, linear in the
    // texels - sprite cache cells are mostly hidden).
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    std::vector<uint8_t> queued(known);
    std::vector<size_t> ring, next;
    const auto addNeighbours = [&](size_t i, std::vector<size_t>& to) {
        const int x = int(i % size_t(width)), y = int(i / size_t(width));
        for (int d = 0; d < 4; ++d) {
            const int xx = x + dx[d], yy = y + dy[d];
            if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
            const size_t j = size_t(yy) * size_t(width) + size_t(xx);
            if (queued[j]) continue;
            queued[j] = 1;
            to.push_back(j);
        }
    };
    for (size_t i = 0; i < n; ++i)
        if (known[i]) addNeighbours(i, ring);
    std::vector<float> value;
    while (!ring.empty()) {
        value.assign(ring.size() * 3, 0.0f);
        for (size_t r = 0; r < ring.size(); ++r) {
            const int x = int(ring[r] % size_t(width)), y = int(ring[r] / size_t(width));
            float s[3] = {0, 0, 0};
            int k = 0;
            for (int d = 0; d < 4; ++d) {
                const int xx = x + dx[d], yy = y + dy[d];
                if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
                const size_t j = size_t(yy) * size_t(width) + size_t(xx);
                if (!known[j]) continue;
                for (int c = 0; c < 3; ++c) s[c] += rgb[j * 3 + c];
                ++k;
            }
            for (int c = 0; c < 3; ++c) value[r * 3 + c] = s[c] / float(k);  // k >= 1: it came from a known one
        }
        for (size_t r = 0; r < ring.size(); ++r) {
            for (int c = 0; c < 3; ++c) rgb[ring[r] * 3 + c] = value[r * 3 + c];
            known[ring[r]] = 1;
        }
        next.clear();
        for (size_t i : ring) addNeighbours(i, next);
        ring.swap(next);
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

void enlargeCell(const uint32_t* argb, int width, int height, int scale, std::vector<uint32_t>& out) {
    std::vector<float> planes;
    prepareInput(argb, width, height, planes);
    std::vector<uint8_t> alpha;
    const size_t n = size_t(width) * size_t(height);
    enlargeAlpha(planes.data() + 3 * n, width, height, scale, alpha);
    const int ow = width * scale, oh = height * scale;
    out.assign(size_t(ow) * size_t(oh), 0);
    for (int y = 0; y < oh; ++y) {
        const float fy = std::clamp((float(y) + 0.5f) / float(scale) - 0.5f, 0.0f, float(height - 1));
        const int y0 = int(fy), y1 = std::min(y0 + 1, height - 1);
        const float ty = fy - float(y0);
        for (int x = 0; x < ow; ++x) {
            const float fx = std::clamp((float(x) + 0.5f) / float(scale) - 0.5f, 0.0f, float(width - 1));
            const int x0 = int(fx), x1 = std::min(x0 + 1, width - 1);
            const float tx = fx - float(x0);
            uint32_t p = uint32_t(alpha[size_t(y) * size_t(ow) + size_t(x)]) << 24;
            for (int c = 0; c < 3; ++c) {
                const float* plane = planes.data() + size_t(c) * n;
                const auto at = [&](int xx, int yy) { return plane[size_t(yy) * size_t(width) + size_t(xx)]; };
                const float v = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) +
                                (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
                p |= uint32_t(std::clamp(int(v * 255.0f + 0.5f), 0, 255)) << (16 - 8 * c);
            }
            out[size_t(y) * size_t(ow) + size_t(x)] = p;
        }
    }
}

void enlargeNearest(const uint32_t* argb, int width, int height, int scale, std::vector<uint32_t>& out) {
    const int ow = width * scale;
    out.resize(size_t(ow) * size_t(height * scale));
    for (int y = 0; y < height * scale; ++y) {
        uint32_t* row = out.data() + size_t(y) * size_t(ow);
        const uint32_t* src = argb + size_t(y / scale) * size_t(width);
        for (int x = 0; x < ow; ++x) row[x] = src[x / scale];
    }
}

void halve(const std::vector<uint32_t>& argb, int width, int height, std::vector<uint32_t>& out, int& outWidth,
           int& outHeight) {
    outWidth = std::max(1, width / 2);
    outHeight = std::max(1, height / 2);
    out.assign(size_t(outWidth) * size_t(outHeight), 0);
    for (int y = 0; y < outHeight; ++y)
        for (int x = 0; x < outWidth; ++x) {
            uint32_t s[4] = {0, 0, 0, 0}, n = 0;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const int sx = x * 2 + dx, sy = y * 2 + dy;
                    if (sx >= width || sy >= height) continue;
                    const uint32_t p = argb[size_t(sy) * size_t(width) + size_t(sx)];
                    for (int c = 0; c < 4; ++c) s[c] += (p >> (8 * c)) & 255;  // b, g, r, a
                    ++n;
                }
            uint32_t m[4];
            for (int c = 0; c < 4; ++c) m[c] = (s[c] + n / 2) / std::max(n, 1u);
            const uint32_t a = m[3] >= 128 ? 255u : 0u;
            out[size_t(y) * size_t(outWidth) + size_t(x)] = a << 24 | m[2] << 16 | m[1] << 8 | m[0];
        }
}

}  // namespace uagfx::ai
