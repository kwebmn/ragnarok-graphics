#include "core/surface_ops.hpp"

namespace uagfx {
namespace {

bool inside(const Rect& r, int width, int height) {
    return !r.empty() && r.left >= 0 && r.top >= 0 && r.right <= width && r.bottom <= height;
}

// The sr part of src as its own image in format f.
Image region(const Image& src, const Rect& sr, PixelFormat f) {
    Image out(sr.width(), sr.height(), f);
    for (int y = 0; y < sr.height(); ++y)
        for (int x = 0; x < sr.width(); ++x)
            out.setPixel(x, y, fromArgb8(f, toArgb8(src.format(), src.pixel(sr.left + x, sr.top + y))));
    return out;
}

}  // namespace

uint8_t* pixelAddress(Image& img, int x, int y) {
    return img.row(y) + size_t(x) * size_t(bytesPerPixel(img.format()));
}

void toArgbRect(const Image& img, const Rect& r, const ColorKey& key, std::vector<uint32_t>& out) {
    out.resize(size_t(r.width()) * size_t(r.height()));
    size_t i = 0;
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x) {
            const uint32_t raw = img.pixel(x, y);
            out[i++] = key.matches(raw) ? 0u : toArgb8(img.format(), raw);
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
    for (int y = 0; y < sr.height(); ++y)
        for (int x = 0; x < sr.width(); ++x)
            part.setPixel(x, y, targetToClient(dst.format(),
                                               target[size_t(sr.top + y) * size_t(width) + size_t(sr.left + x)]));
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

}  // namespace uagfx
