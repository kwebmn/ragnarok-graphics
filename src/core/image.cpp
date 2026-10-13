#include "core/image.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

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

uint32_t lerpArgb(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const float ca = float((a >> shift) & 255);
        const float cb = float((b >> shift) & 255);
        out |= uint32_t(std::lround(ca + (cb - ca) * t)) << shift;
    }
    return out;
}

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
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x) dst.setPixel(x, y, raw);
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
    for (int y = 0; y < sr.height(); ++y)
        for (int x = 0; x < sr.width(); ++x) {
            const uint32_t v = src.pixel(sr.left + x, sr.top + y);
            if (v < keyLow || v > keyHigh) dst.setPixel(dx + x, dy + y, v);
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
    for (int y = dr.top; y < dr.bottom; ++y) {
        for (int x = dr.left; x < dr.right; ++x) {
            const float u = (float(x - full.left) + 0.5f) * sx;  // source position of the pixel centre
            const float v = (float(y - full.top) + 0.5f) * sy;
            if (filter == StretchFilter::Point) {
                const int px = std::clamp(sr.left + int(u), sr.left, sr.right - 1);
                const int py = std::clamp(sr.top + int(v), sr.top, sr.bottom - 1);
                dst.setPixel(x, y, src.pixel(px, py));
                continue;
            }
            const float fu = u - 0.5f;
            const float fv = v - 0.5f;
            const int x0 = int(std::floor(fu));
            const int y0 = int(std::floor(fv));
            const float tx = fu - float(x0);
            const float ty = fv - float(y0);
            auto at = [&](int ix, int iy) {
                ix = std::clamp(sr.left + ix, sr.left, sr.right - 1);
                iy = std::clamp(sr.top + iy, sr.top, sr.bottom - 1);
                return toArgb8(f, src.pixel(ix, iy));
            };
            const uint32_t top = lerpArgb(at(x0, y0), at(x0 + 1, y0), tx);
            const uint32_t bottom = lerpArgb(at(x0, y0 + 1), at(x0 + 1, y0 + 1), tx);
            dst.setPixel(x, y, fromArgb8(f, lerpArgb(top, bottom, ty)));
        }
    }
    return true;
}

}  // namespace uagfx
