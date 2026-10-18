#include "core/full_colour.hpp"

#include <algorithm>

namespace uagfx::full_colour {

bool shortFormat(PixelFormat f) {
    return f == PixelFormat::R5G6B5 || f == PixelFormat::X1R5G5B5 || f == PixelFormat::A1R5G5B5 ||
           f == PixelFormat::A4R4G4B4;
}

Write writeOf(int x, int y, int w, int h, int sw, int sh) {
    Write out;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || sw <= 0 || sh <= 0) return out;
    while (w / out.stepX > sw) ++out.stepX;
    while (h / out.stepY > sh) ++out.stepY;
    const int left = x / out.stepX, top = y / out.stepY;
    out.rect = Rect{left, top, left + w / out.stepX, top + h / out.stepY};
    return out;
}

void texelsOf(const uint32_t* picture, int pitch, const Write& at, std::vector<uint32_t>& out) {
    const int w = at.rect.width(), h = at.rect.height();
    out.clear();
    if (!picture || w <= 0 || h <= 0) return;
    out.resize(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y) {
        const uint32_t* row = picture + size_t(y) * size_t(at.stepY) * size_t(pitch);
        uint32_t* to = out.data() + size_t(y) * size_t(w);
        for (int x = 0; x < w; ++x) to[x] = row[size_t(x) * size_t(at.stepX)];
    }
}

uint32_t merge(PixelFormat f, uint32_t raw, uint32_t argb, uint32_t full) {
    if ((argb >> 24) == 0) return argb;  // keyed away: as the game has it
    const uint32_t bits = f == PixelFormat::X1R5G5B5 ? 0x7FFFu : 0xFFFFu;
    if ((fromArgb8(f, full) & bits) != (raw & bits)) return argb;  // written again since
    if (f == PixelFormat::A4R4G4B4) return full;
    return (argb & 0xFF000000u) | (full & 0x00FFFFFFu);
}

uint32_t mergeMip(PixelFormat f, uint32_t argb, uint32_t full) {
    if ((argb >> 24) == 0 || (full >> 24) == 0) return argb;
    if (f == PixelFormat::A4R4G4B4) return full;
    return (argb & 0xFF000000u) | (full & 0x00FFFFFFu);
}

void halve(const std::vector<uint32_t>& in, int w, int h, std::vector<uint32_t>& out, int& ow, int& oh) {
    ow = std::max(1, w / 2);
    oh = std::max(1, h / 2);
    out.assign(size_t(ow) * size_t(oh), 0);
    if (w <= 0 || h <= 0 || in.size() < size_t(w) * size_t(h)) return;
    for (int y = 0; y < oh; ++y)
        for (int x = 0; x < ow; ++x) {
            uint32_t sa = 0, sr = 0, sg = 0, sb = 0;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const int xx = std::min(2 * x + dx, w - 1), yy = std::min(2 * y + dy, h - 1);
                    const uint32_t p = in[size_t(yy) * size_t(w) + size_t(xx)], a = p >> 24;
                    sa += a;
                    sr += (p >> 16 & 255) * a;
                    sg += (p >> 8 & 255) * a;
                    sb += (p & 255) * a;
                }
            if (!sa) continue;  // nothing shows: none of its colour
            const uint32_t r = (sr + sa / 2) / sa, g = (sg + sa / 2) / sa, b = (sb + sa / 2) / sa;
            out[size_t(y) * size_t(ow) + size_t(x)] = (sa + 2) / 4 << 24 | r << 16 | g << 8 | b;
        }
}

uint32_t gameTexel(PixelFormat f, uint32_t texel) {
    if (!shortFormat(f) || (texel >> 24) == 0) return texel;  // a transparent texel goes up as the game's
    return toArgb8(f, fromArgb8(f, texel));
}

void gameView(PixelFormat f, const uint32_t* texels, size_t count, std::vector<uint32_t>& out) {
    out.resize(count);
    for (size_t i = 0; i < count; ++i) out[i] = gameTexel(f, texels[i]);
}

const uint32_t* aiTexels(const UploadCmd& u, const uint32_t* pixels, std::vector<uint32_t>& keep) {
    if (!u.fullFrom || u.level != 0 || !pixels) return pixels;  // the AI textures take level 0 alone
    gameView(PixelFormat(u.fullFrom), pixels, size_t(u.rect.width()) * size_t(u.rect.height()), keep);
    return keep.data();
}

}  // namespace uagfx::full_colour
