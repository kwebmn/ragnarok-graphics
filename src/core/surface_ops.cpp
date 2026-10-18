#include "core/surface_ops.hpp"

#include <algorithm>

namespace uagfx {
namespace {

bool inside(const Rect& r, int width, int height) {
    return !r.empty() && r.left >= 0 && r.top >= 0 && r.right <= width && r.bottom <= height;
}

// The sr part of src as its own image in format f (2.5.11: a row at a time, each format chosen once a row - the
// game's sprite frames come this way, a Blt between formats).
Image region(const Image& src, const Rect& sr, PixelFormat f) {
    Image out(sr.width(), sr.height(), f);
    std::vector<uint32_t> argb(size_t(sr.width()));
    const size_t from = size_t(sr.left) * size_t(bytesPerPixel(src.format()));
    for (int y = 0; y < sr.height(); ++y) {
        rowToArgb8(src.format(), src.row(sr.top + y) + from, argb.data(), sr.width());
        rowFromArgb8(f, argb.data(), out.row(y), sr.width());
    }
    return out;
}

}  // namespace

uint8_t* pixelAddress(Image& img, int x, int y) {
    return img.row(y) + size_t(x) * size_t(bytesPerPixel(img.format()));
}

void toArgbRect(const Image& img, const Rect& r, const ColorKey& key, std::vector<uint32_t>& out) {
    out.resize(size_t(r.width()) * size_t(r.height()));
    const int w = r.width();
    const size_t from = size_t(r.left) * size_t(bytesPerPixel(img.format()));
    for (int y = r.top; y < r.bottom; ++y) {  // 2.5.11: a row at a time, the format chosen once a row
        uint32_t* to = out.data() + size_t(y - r.top) * size_t(w);
        if (key.enabled)
            rowToArgb8Keyed(img.format(), img.row(y) + from, to, w, key.low, key.high);
        else
            rowToArgb8(img.format(), img.row(y) + from, to, w);
    }
}

bool blitCpu(Image& dst, const Rect& dr, const Image& src, const Rect& sr, const ColorKey& key) {
    if (dst.format() == PixelFormat::Unknown || src.format() == PixelFormat::Unknown) return false;
    if (dr.empty() || !inside(sr, src.width(), src.height())) return false;
    if (src.format() != dst.format()) {
        const Image part = region(src, sr, dst.format());
        ColorKey k = key;
        k.low = fromArgb8(dst.format(), toArgb8(src.format(), key.low));
        k.high = fromArgb8(dst.format(), toArgb8(src.format(), key.high));
        return blitCpu(dst, dr, part, part.bounds(), k);
    }
    if (dr.width() == sr.width() && dr.height() == sr.height())
        return key.enabled ? copyKeyed(dst, dr.left, dr.top, src, sr, key.low, key.high)
                           : copy(dst, dr.left, dr.top, src, sr);
    if (!key.enabled) return stretch(dst, dr, src, sr, StretchFilter::Linear);
    Image scaled(dr.width(), dr.height(), src.format());
    stretch(scaled, scaled.bounds(), src, sr, StretchFilter::Point);
    return copyKeyed(dst, dr.left, dr.top, scaled, scaled.bounds(), key.low, key.high);
}

bool readTargetInto(Image& dst, const Rect& dr, const std::vector<uint32_t>& target, int width, int height,
                    const Rect& sr) {
    if (dst.format() == PixelFormat::Unknown || dr.empty() || !inside(sr, width, height)) return false;
    if (target.size() < size_t(width) * size_t(height)) return false;
    Image part(sr.width(), sr.height(), dst.format());
    for (int y = 0; y < sr.height(); ++y)  // 2.5.11: a row at a time
        rowTargetToClient(dst.format(), target.data() + size_t(sr.top + y) * size_t(width) + size_t(sr.left),
                          part.row(y), sr.width());
    if (dr.width() == part.width() && dr.height() == part.height())
        return copy(dst, dr.left, dr.top, part, part.bounds());
    return stretch(dst, dr, part, part.bounds(), StretchFilter::Point);
}

bool targetWrite(const Image& src, const Rect& sr, const Rect& dr, int width, int height, const ColorKey& key,
                 const std::vector<uint32_t>& current, TargetWrite& out) {
    if (src.format() == PixelFormat::Unknown || dr.empty() || !inside(sr, src.width(), src.height())) return false;
    if (key.enabled && current.size() < size_t(width) * size_t(height)) return false;
    out.rect = dr;
    out.argb.clear();
    if (!clipRect(Rect{0, 0, width, height}, out.rect)) {
        out.rect = Rect{};
        return true;
    }
    const bool same = dr.width() == sr.width() && dr.height() == sr.height();
    Image scaled;
    if (!same) {
        scaled = Image(dr.width(), dr.height(), src.format());
        stretch(scaled, scaled.bounds(), src, sr, key.enabled ? StretchFilter::Point : StretchFilter::Linear);
    }
    out.argb.reserve(size_t(out.rect.width()) * size_t(out.rect.height()));
    for (int y = out.rect.top; y < out.rect.bottom; ++y)
        for (int x = out.rect.left; x < out.rect.right; ++x) {
            const uint32_t raw = same ? src.pixel(sr.left + x - dr.left, sr.top + y - dr.top)
                                      : scaled.pixel(x - dr.left, y - dr.top);
            out.argb.push_back(key.matches(raw) ? current[size_t(y) * size_t(width) + size_t(x)]
                                                : toArgb8(src.format(), raw));
        }
    return true;
}

Rect changedRect(const uint32_t* before, const uint32_t* after, int width, int height) {
    Rect r{width, height, 0, 0};
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const size_t i = size_t(y) * size_t(width) + size_t(x);
            if (((before[i] ^ after[i]) & 0x00FFFFFFu) == 0) continue;
            r.left = std::min(r.left, x);
            r.top = std::min(r.top, y);
            r.right = std::max(r.right, x + 1);
            r.bottom = std::max(r.bottom, y + 1);
        }
    return r.empty() ? Rect{} : r;
}

}  // namespace uagfx
