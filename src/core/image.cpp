#include "core/image.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace uagfx {
namespace {

// DirectDraw answers DDERR_INVALIDRECT unless the source rectangle lies inside its surface.
bool inside(const Rect& r, const Image& img) {
    return !r.empty() && r.left >= 0 && r.top >= 0 && r.right <= img.width() && r.bottom <= img.height();
}

// Moves (dx, dy, sr) so that both the source and the destination rectangles lie inside their images.
bool clipCopy(const Image& dst, int& dx, int& dy, const Image& src, Rect& sr) {
    if (!clipRect(src.bounds(), sr)) return false;
    Rect dr{dx, dy, dx + sr.width(), dy + sr.height()};
    if (!clipRect(dst.bounds(), dr)) return false;
    sr.left += dr.left - dx;
    sr.top += dr.top - dy;
    sr.right = sr.left + dr.width();
    sr.bottom = sr.top + dr.height();
    dx = dr.left;
    dy = dr.top;
    return true;
}

// A row of pixels of type T copied but those whose value lies in [low, high] (copyKeyed's, left to right).
template <typename T>
void keyedRow(const uint8_t* from, uint8_t* to, int count, uint32_t low, uint32_t high) {
    for (int i = 0; i < count; ++i) {
        T v;
        std::memcpy(&v, from + sizeof(T) * size_t(i), sizeof(T));
        if (uint32_t(v) < low || uint32_t(v) > high) std::memcpy(to + sizeof(T) * size_t(i), &v, sizeof(T));
    }
}

// std::lround of a channel's value (0 to 255): its halves up. 2.5.14: without the library's call - lroundf was a
// third of the game's thread (R-7NUW85), twelve a pixel of the mip levels the game stretches its textures into.
inline uint32_t roundChannel(float v) {
    const int whole = int(v);
    return uint32_t(whole + (v - float(whole) >= 0.5f ? 1 : 0));  // v - whole is exact below 2^23
}

uint32_t lerpArgb(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const float ca = float((a >> shift) & 255);
        const float cb = float((b >> shift) & 255);
        out |= roundChannel(ca + (cb - ca) * t) << shift;
    }
    return out;
}

// A destination column's or row's place in the source (stretch): the two pixels around it, clamped to the source
// rectangle, and the second's weight.
struct Taps {
    int first = 0, second = 0;
    float t = 0.0f;
};

}  // namespace

bool clipRect(const Rect& bounds, Rect& r) {
    r.left = std::max(r.left, bounds.left);
    r.top = std::max(r.top, bounds.top);
    r.right = std::min(r.right, bounds.right);
    r.bottom = std::min(r.bottom, bounds.bottom);
    return !r.empty();
}

Image::Image(int width, int height, PixelFormat format)
    : w_(width), h_(height), f_(format), pitch_((width * bytesPerPixel(format) + 3) & ~3),
      px_(size_t(pitch_) * size_t(height)) {}

void fill(Image& dst, Rect r, uint32_t raw) {
    if (!clipRect(dst.bounds(), r)) return;
    // 2.5.11: one row made, then copied into each
    const size_t bpp = size_t(bytesPerPixel(dst.format())), bytes = size_t(r.width()) * bpp;
    if (!bytes) return;
    std::vector<uint8_t> row(bytes);
    for (size_t i = 0; i < bytes; i += bpp) writePixel(dst.format(), row.data() + i, raw);
    for (int y = r.top; y < r.bottom; ++y) std::memcpy(dst.row(y) + size_t(r.left) * bpp, row.data(), bytes);
}

bool copy(Image& dst, int dx, int dy, const Image& src, Rect sr) {
    if (dst.format() != src.format() || dst.format() == PixelFormat::Unknown) return false;
    if (!inside(sr, src)) return false;
    if (!clipCopy(dst, dx, dy, src, sr)) return true;  // nothing visible: like DirectDraw, not an error
    const int bpp = bytesPerPixel(src.format());
    const size_t bytes = size_t(sr.width()) * size_t(bpp);
    const bool bottomUp = &dst == &src && dy > sr.top;  // overlapping rows: copy from the far end
    for (int i = 0; i < sr.height(); ++i) {
        const int y = bottomUp ? sr.height() - 1 - i : i;
        std::memmove(dst.row(dy + y) + dx * bpp, src.row(sr.top + y) + sr.left * bpp, bytes);
    }
    return true;
}

bool copyKeyed(Image& dst, int dx, int dy, const Image& src, Rect sr, uint32_t keyLow, uint32_t keyHigh) {
    if (dst.format() != src.format() || dst.format() == PixelFormat::Unknown) return false;
    if (!inside(sr, src)) return false;
    if (!clipCopy(dst, dx, dy, src, sr)) return true;
    const size_t bpp = size_t(bytesPerPixel(src.format()));
    for (int y = 0; y < sr.height(); ++y) {  // 2.5.11: the pixel's size chosen once a row, not each pixel
        const uint8_t* from = src.row(sr.top + y) + size_t(sr.left) * bpp;
        uint8_t* to = dst.row(dy + y) + size_t(dx) * bpp;
        if (bpp == 2)
            keyedRow<uint16_t>(from, to, sr.width(), keyLow, keyHigh);
        else
            keyedRow<uint32_t>(from, to, sr.width(), keyLow, keyHigh);
    }
    return true;
}

bool stretch(Image& dst, Rect dr, const Image& src, Rect sr, StretchFilter filter) {
    if (dst.format() != src.format() || dst.format() == PixelFormat::Unknown) return false;
    if (dr.empty() || !inside(sr, src)) return false;
    const Rect full = dr;  // the mapping is defined by the unclipped rectangles
    if (!clipRect(dst.bounds(), dr)) return true;
    const PixelFormat f = src.format();
    const float sx = float(sr.width()) / float(full.width());
    const float sy = float(sr.height()) / float(full.height());
    // The source position of a pixel's centre along one axis - the same for a whole column or row, so worked out
    // once for it (2.5.14: it was for each pixel): the pixel itself (Point) or the two around it (Linear).
    const auto taps = [filter](int i, int from, float scale, int low, int high) {
        const float u = (float(i - from) + 0.5f) * scale;
        Taps k;
        if (filter == StretchFilter::Point) {
            k.first = k.second = std::clamp(low + int(u), low, high - 1);
            return k;
        }
        const float fu = u - 0.5f;
        const int i0 = int(std::floor(fu));
        k.first = std::clamp(low + i0, low, high - 1);
        k.second = std::clamp(low + i0 + 1, low, high - 1);
        k.t = fu - float(i0);
        return k;
    };
    std::vector<Taps> columns(size_t(dr.width()));
    for (int x = dr.left; x < dr.right; ++x) columns[size_t(x - dr.left)] = taps(x, full.left, sx, sr.left, sr.right);
    for (int y = dr.top; y < dr.bottom; ++y) {
        const Taps row = taps(y, full.top, sy, sr.top, sr.bottom);
        if (filter == StretchFilter::Point) {
            for (int x = dr.left; x < dr.right; ++x)
                dst.setPixel(x, y, src.pixel(columns[size_t(x - dr.left)].first, row.first));
            continue;
        }
        for (int x = dr.left; x < dr.right; ++x) {
            const Taps& c = columns[size_t(x - dr.left)];
            const uint32_t top = lerpArgb(toArgb8(f, src.pixel(c.first, row.first)),
                                          toArgb8(f, src.pixel(c.second, row.first)), c.t);
            const uint32_t bottom = lerpArgb(toArgb8(f, src.pixel(c.first, row.second)),
                                             toArgb8(f, src.pixel(c.second, row.second)), c.t);
            dst.setPixel(x, y, fromArgb8(f, lerpArgb(top, bottom, row.t)));
        }
    }
    return true;
}

}  // namespace uagfx
