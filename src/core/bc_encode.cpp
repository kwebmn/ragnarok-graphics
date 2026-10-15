#include "core/bc_encode.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::bc {
namespace {

struct Rgb {
    int r, g, b;
};

uint16_t to565(int r, int g, int b) {
    return uint16_t((std::clamp((r * 31 + 127) / 255, 0, 31) << 11) | (std::clamp((g * 63 + 127) / 255, 0, 63) << 5) |
                    std::clamp((b * 31 + 127) / 255, 0, 31));
}

Rgb from565(uint16_t c) {
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return Rgb{(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)};
}

// The block at (bx, by): 16 texels, the ones past the picture's edge repeating its last row and column.
void gather(const uint32_t* argb, int width, int height, int bx, int by, uint32_t out[16]) {
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            const int px = std::min(bx * 4 + x, width - 1), py = std::min(by * 4 + y, height - 1);
            out[y * 4 + x] = argb[size_t(py) * size_t(width) + size_t(px)];
        }
}

// BC1's colour half: two 565 ends along the texels' main axis (by their bounding box, inset), 2-bit indexes.
void colourBlock(const uint32_t px[16], uint8_t* out) {
    int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
    for (int i = 0; i < 16; ++i) {
        const int c[3] = {int(px[i] >> 16 & 255), int(px[i] >> 8 & 255), int(px[i] & 255)};
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], c[k]);
            hi[k] = std::max(hi[k], c[k]);
        }
    }
    // The diagonal of the box that follows the colours: flip an axis whose values fall as the main one's rise.
    int main = 0;
    for (int k = 1; k < 3; ++k)
        if (hi[k] - lo[k] > hi[main] - lo[main]) main = k;
    int a[3], b[3];
    for (int k = 0; k < 3; ++k) {
        a[k] = hi[k];
        b[k] = lo[k];
    }
    for (int k = 0; k < 3; ++k) {
        if (k == main) continue;
        long cov = 0;
        int mm = 0, mk = 0;
        for (int i = 0; i < 16; ++i) {
            const int c[3] = {int(px[i] >> 16 & 255), int(px[i] >> 8 & 255), int(px[i] & 255)};
            mm += c[main];
            mk += c[k];
        }
        for (int i = 0; i < 16; ++i) {
            const int c[3] = {int(px[i] >> 16 & 255), int(px[i] >> 8 & 255), int(px[i] & 255)};
            cov += long(c[main] * 16 - mm) * long(c[k] * 16 - mk);
        }
        if (cov < 0) std::swap(a[k], b[k]);
    }
    for (int k = 0; k < 3; ++k) {  // inset by a sixteenth: the ends' rounding hurts less
        const int inset = (a[k] - b[k]) / 16;
        a[k] -= inset;
        b[k] += inset;
    }
    uint16_t c0 = to565(a[0], a[1], a[2]), c1 = to565(b[0], b[1], b[2]);
    if (c0 < c1) std::swap(c0, c1);  // four colours (c0 > c1)
    uint32_t indexes = 0;
    if (c0 != c1) {
        const Rgb e0 = from565(c0), e1 = from565(c1);
        const Rgb pal[4] = {e0, e1, {(2 * e0.r + e1.r) / 3, (2 * e0.g + e1.g) / 3, (2 * e0.b + e1.b) / 3},
                            {(e0.r + 2 * e1.r) / 3, (e0.g + 2 * e1.g) / 3, (e0.b + 2 * e1.b) / 3}};
        for (int i = 0; i < 16; ++i) {
            const int r = int(px[i] >> 16 & 255), g = int(px[i] >> 8 & 255), bl = int(px[i] & 255);
            int best = 0, bestD = 1 << 30;
            for (int p = 0; p < 4; ++p) {
                const int d = (r - pal[p].r) * (r - pal[p].r) + (g - pal[p].g) * (g - pal[p].g) +
                              (bl - pal[p].b) * (bl - pal[p].b);
                if (d < bestD) {
                    bestD = d;
                    best = p;
                }
            }
            indexes |= uint32_t(best) << (2 * i);
        }
    }
    out[0] = uint8_t(c0);
    out[1] = uint8_t(c0 >> 8);
    out[2] = uint8_t(c1);
    out[3] = uint8_t(c1 >> 8);
    std::memcpy(out + 4, &indexes, 4);
}

// BC3's alpha half: two 8-bit ends (min, max), eight levels, 3-bit indexes.
void alphaBlock(const uint32_t px[16], uint8_t* out) {
    int lo = 255, hi = 0;
    for (int i = 0; i < 16; ++i) {
        lo = std::min(lo, int(px[i] >> 24));
        hi = std::max(hi, int(px[i] >> 24));
    }
    out[0] = uint8_t(hi);
    out[1] = uint8_t(lo);
    uint64_t bits = 0;
    if (hi != lo) {
        int level[8] = {hi, lo};
        for (int k = 1; k < 7; ++k) level[k + 1] = ((7 - k) * hi + k * lo) / 7;
        for (int i = 0; i < 16; ++i) {
            const int a = int(px[i] >> 24);
            int best = 0, bestD = 1 << 30;
            for (int k = 0; k < 8; ++k)
                if (std::abs(a - level[k]) < bestD) {
                    bestD = std::abs(a - level[k]);
                    best = k;
                }
            bits |= uint64_t(best) << (3 * i);
        }
    }
    for (int k = 0; k < 6; ++k) out[2 + k] = uint8_t(bits >> (8 * k));
}

}  // namespace

bool opaque(const uint32_t* argb, int width, int height) {
    for (size_t i = 0; i < size_t(width) * size_t(height); ++i)
        if ((argb[i] >> 24) != 255) return false;
    return true;
}

std::vector<uint8_t> encodeBC1(const uint32_t* argb, int width, int height) {
    const int bw = blocksAcross(width), bh = blocksAcross(height);
    std::vector<uint8_t> out(size_t(bw) * size_t(bh) * 8);
    uint32_t px[16];
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            gather(argb, width, height, bx, by, px);
            colourBlock(px, &out[(size_t(by) * size_t(bw) + size_t(bx)) * 8]);
        }
    return out;
}

std::vector<uint8_t> encodeBC3(const uint32_t* argb, int width, int height) {
    const int bw = blocksAcross(width), bh = blocksAcross(height);
    std::vector<uint8_t> out(size_t(bw) * size_t(bh) * 16);
    uint32_t px[16];
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            gather(argb, width, height, bx, by, px);
            uint8_t* block = &out[(size_t(by) * size_t(bw) + size_t(bx)) * 16];
            alphaBlock(px, block);
            colourBlock(px, block + 8);
        }
    return out;
}

std::vector<uint32_t> decodeBC1(const uint8_t* blocks, int width, int height) {
    std::vector<uint32_t> out(size_t(width) * size_t(height));
    const int bw = blocksAcross(width);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const uint8_t* b = blocks + (size_t(y / 4) * size_t(bw) + size_t(x / 4)) * 8;
            const uint16_t c0 = uint16_t(b[0] | b[1] << 8), c1 = uint16_t(b[2] | b[3] << 8);
            const Rgb e0 = from565(c0), e1 = from565(c1);
            uint32_t idx = 0;
            std::memcpy(&idx, b + 4, 4);
            const int i = int(idx >> (2 * ((y % 4) * 4 + x % 4)) & 3);
            Rgb c = i == 0 ? e0 : i == 1 ? e1 : Rgb{0, 0, 0};
            uint32_t a = 255;
            if (c0 > c1) {
                if (i == 2) c = {(2 * e0.r + e1.r) / 3, (2 * e0.g + e1.g) / 3, (2 * e0.b + e1.b) / 3};
                if (i == 3) c = {(e0.r + 2 * e1.r) / 3, (e0.g + 2 * e1.g) / 3, (e0.b + 2 * e1.b) / 3};
            } else {
                if (i == 2) c = {(e0.r + e1.r) / 2, (e0.g + e1.g) / 2, (e0.b + e1.b) / 2};
                if (i == 3) a = 0;
            }
            out[size_t(y) * size_t(width) + size_t(x)] =
                a << 24 | uint32_t(c.r) << 16 | uint32_t(c.g) << 8 | uint32_t(c.b);
        }
    return out;
}

std::vector<uint32_t> decodeBC3(const uint8_t* blocks, int width, int height) {
    const int bw = blocksAcross(width), bh = blocksAcross(height);
    std::vector<uint8_t> colour(size_t(bw) * size_t(bh) * 8);
    for (size_t i = 0; i < size_t(bw) * size_t(bh); ++i) std::memcpy(&colour[i * 8], blocks + i * 16 + 8, 8);
    std::vector<uint32_t> out = decodeBC1(colour.data(), width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const uint8_t* b = blocks + (size_t(y / 4) * size_t(bw) + size_t(x / 4)) * 16;
            int level[8] = {b[0], b[1]};
            if (b[0] > b[1])
                for (int k = 1; k < 7; ++k) level[k + 1] = ((7 - k) * b[0] + k * b[1]) / 7;
            else {
                for (int k = 1; k < 5; ++k) level[k + 1] = ((5 - k) * b[0] + k * b[1]) / 5;
                level[6] = 0;
                level[7] = 255;
            }
            uint64_t bits = 0;
            for (int k = 0; k < 6; ++k) bits |= uint64_t(b[2 + k]) << (8 * k);
            const int i = int(bits >> (3 * ((y % 4) * 4 + x % 4)) & 7);
            uint32_t& p = out[size_t(y) * size_t(width) + size_t(x)];
            p = (p & 0xFFFFFF) | uint32_t(level[i]) << 24;
        }
    return out;
}

}  // namespace uagfx::bc
