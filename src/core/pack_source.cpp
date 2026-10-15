#include "core/pack_source.hpp"

#include <algorithm>
#include <cstring>

#include "core/ai/texture_cache.hpp"
#include "core/pixel.hpp"
#include "core/twin.hpp"

namespace uagfx::pack {
namespace {

uint32_t le16(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; }
uint32_t le32(const uint8_t* p) { return le16(p) | le16(p + 2) << 16; }
bool pow2(int v) { return v > 0 && (v & (v - 1)) == 0; }

void keyed(Picture& p) { p.key = ai::keyName(ai::textureKey(p.argb.data(), p.width, p.height)); }

}  // namespace

bool decodeBmp(const uint8_t* data, size_t size, std::vector<uint32_t>& argb, int& width, int& height) {
    if (!data || size < 0x36 || data[0] != 'B' || data[1] != 'M') return false;
    const uint32_t bits = le32(data + 10);
    const int w = int(le32(data + 18)), h = int(le32(data + 22));
    const uint32_t depth = le16(data + 28), compression = le32(data + 30);
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || compression != 0 || (depth != 8 && depth != 24)) return false;
    const size_t stride = depth == 8 ? (size_t(w) + 3) & ~size_t(3) : (size_t(w) * 3 + 3) & ~size_t(3);
    if (bits > size || stride * size_t(h) > size - bits) return false;
    uint32_t colours = depth == 8 ? le32(data + 46) : 0;
    if (depth == 8 && (colours == 0 || colours > 256)) colours = 256;
    if (depth == 8 && 0x36 + size_t(colours) * 4 > size) return false;
    argb.assign(size_t(w) * size_t(h), 0);
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = data + bits + stride * size_t(h - 1 - y);  // bottom-up
        uint32_t* out = argb.data() + size_t(y) * size_t(w);
        for (int x = 0; x < w; ++x) {
            const uint8_t* bgr = depth == 8 ? data + 0x36 + size_t(row[x]) * 4 : row + size_t(x) * 3;
            out[x] = 0xFF000000u | uint32_t(bgr[2]) << 16 | uint32_t(bgr[1]) << 8 | bgr[0];
        }
    }
    width = w;
    height = h;
    return true;
}

bool decodeTga(const uint8_t* data, size_t size, std::vector<uint32_t>& argb, int& width, int& height, bool& alpha) {
    if (!data || size < 18) return false;
    const uint32_t idLength = data[0], mapType = data[1], type = data[2], depth = data[16], descriptor = data[17];
    const int w = int(le16(data + 12)), h = int(le16(data + 14));
    if (mapType != 0 || (type != 2 && type != 10) || (depth != 24 && depth != 32) || w <= 0 || h <= 0) return false;
    const size_t bpp = depth / 8, n = size_t(w) * size_t(h);
    size_t at = 18 + idLength;
    std::vector<uint32_t> pixels(n);
    const auto pixel = [&](size_t i) -> uint32_t {
        const uint8_t* p = data + i;
        return (bpp == 4 ? uint32_t(p[3]) : 255u) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0];
    };
    if (type == 2) {
        if (at > size || n * bpp > size - at) return false;
        for (size_t i = 0; i < n; ++i) pixels[i] = pixel(at + i * bpp);
    } else {
        for (size_t i = 0; i < n;) {
            if (at >= size) return false;
            const uint32_t head = data[at++];
            const size_t count = (head & 127) + 1;
            if (count > n - i) return false;
            if (head & 128) {
                if (bpp > size - at) return false;
                const uint32_t v = pixel(at);
                at += bpp;
                for (size_t k = 0; k < count; ++k) pixels[i++] = v;
            } else {
                if (count * bpp > size - at) return false;
                for (size_t k = 0; k < count; ++k, at += bpp) pixels[i++] = pixel(at);
            }
        }
    }
    argb.assign(n, 0);
    const bool top = (descriptor & 0x20) != 0;  // else bottom-up
    for (int y = 0; y < h; ++y)
        std::memcpy(argb.data() + size_t(y) * size_t(w), pixels.data() + size_t(top ? y : h - 1 - y) * size_t(w),
                    size_t(w) * 4);
    width = w;
    height = h;
    alpha = bpp == 4;
    return true;
}

uint32_t gameA1R5G5B5(uint32_t c, bool blackKey) {
    const uint32_t v = ((c >> 6) & 0x3e0) + ((c >> 3) & 0x1f) + ((c >> 16) & 0x8000) + ((c >> 9) & 0x7c00);
    if (v == 0xfc1f || (blackKey && (v & 0x7fff) == 0)) return 0;
    return v;
}

uint32_t gameA4R4G4B4(uint32_t c, bool blackKey) {
    const uint32_t v = ((c >> 12) & 0xf00) + ((c >> 8) & 0xf0) + ((c >> 4) & 0xf) + ((c >> 16) & 0xf000);
    if ((v & 0xfff) == 0xf0f || (blackKey && (v & 0xfff) == 0)) return 0;
    return v;
}

uint32_t spritePaletteColour(const uint8_t* p) {
    if (p[0] >= 0xFE && p[1] <= 3 && p[2] >= 0xFE) return 0x421;  // magenta: the game's own value (alpha 0)
    return 0x8000u | uint32_t(p[0] >> 3) << 10 | uint32_t(p[1] >> 3) << 5 | uint32_t(p[2] >> 3);
}

int spriteCell(int side) { return side <= 32 ? 32 : side <= 64 ? 64 : side <= 128 ? 128 : side <= 256 ? 256 : 0; }

bool worldTexture(const uint8_t* data, size_t size, bool tga, Picture& out) {
    std::vector<uint32_t> src;
    int w = 0, h = 0;
    bool alpha = false;
    if (!(tga ? decodeTga(data, size, src, w, h, alpha) : decodeBmp(data, size, src, w, h)) || !pow2(w) || !pow2(h))
        return false;
    out.group = Group::World;
    out.width = w;
    out.height = h;
    out.argb.resize(src.size());
    for (size_t i = 0; i < src.size(); ++i)
        out.argb[i] = alpha ? toArgb8(PixelFormat::A4R4G4B4, gameA4R4G4B4(src[i], false))
                            : toArgb8(PixelFormat::A1R5G5B5, gameA1R5G5B5(src[i], false));
    keyed(out);
    return true;
}

bool interfacePicture(const uint8_t* data, size_t size, Picture& out) {
    if (!decodeBmp(data, size, out.argb, out.width, out.height)) return false;
    for (uint32_t& p : out.argb) p = transparentAtUpload(p) ? 0u : (p | 0xFF000000u);
    out.group = Group::Ui;
    keyed(out);
    return true;
}

bool spriteFrames(const uint8_t* data, size_t size, std::vector<Picture>& out, const uint8_t* palette) {
    out.clear();
    if (!data || size < 8 || data[0] != 'S' || data[1] != 'P') return false;
    const uint32_t version = uint32_t(data[3]) << 8 | data[2];
    const uint32_t frames = le16(data + 4);
    size_t at = version >= 0x200 ? 8 : 6;
    if (version <= 0x100 && !palette) return false;  // no palette of its own
    const uint8_t* pal = palette ? palette : (size >= 1024 ? data + size - 1024 : nullptr);
    if (!pal) return false;
    uint32_t colours[256];
    for (int i = 0; i < 256; ++i) colours[i] = toArgb8(PixelFormat::A1R5G5B5, spritePaletteColour(pal + i * 4));
    for (uint32_t f = 0; f < frames; ++f) {
        if (at + 4 > size) return false;
        const int w = int(le16(data + at)), h = int(le16(data + at + 2));
        at += 4;
        size_t length = size_t(w) * size_t(h);
        const bool rle = version >= 0x201;
        if (rle) {
            if (at + 2 > size) return false;
            length = le16(data + at);
            at += 2;
        }
        if (length > size - at) return false;
        const uint8_t* src = data + at;
        at += length;
        const int cw = spriteCell(w), ch = spriteCell(h);
        if (w <= 0 || h <= 0 || !cw || !ch) continue;  // not in the sprite cache
        Picture p;
        p.group = Group::Sprites;
        p.width = cw;
        p.height = ch;
        p.argb.assign(size_t(cw) * size_t(ch), 0);
        size_t i = 0, k = 0;
        const size_t pixels = size_t(w) * size_t(h);
        while (i < pixels && k < length) {
            const uint8_t index = src[k++];
            if (index == 0 && rle) {  // a run of transparent pixels: its length next
                const size_t run = k < length ? std::max<size_t>(src[k++], 1) : 1;
                i += run;
                continue;
            }
            if (index) p.argb[(i / size_t(w)) * size_t(cw) + i % size_t(w)] = colours[index];
            ++i;
        }
        keyed(p);
        out.push_back(std::move(p));
    }
    return true;
}

}  // namespace uagfx::pack
