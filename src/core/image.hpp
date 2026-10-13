#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/pixel.hpp"

namespace uagfx {

struct Rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int width() const { return right - left; }
    int height() const { return bottom - top; }
    bool empty() const { return right <= left || bottom <= top; }
    bool operator==(const Rect& o) const {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
};

// r := r intersected with bounds; false when nothing is left.
bool clipRect(const Rect& bounds, Rect& r);

// The CPU copy of a DirectDraw surface: pixels in the client's format, rows aligned to 4 bytes.
class Image {
public:
    Image() = default;
    Image(int width, int height, PixelFormat format);
    int width() const { return w_; }
    int height() const { return h_; }
    PixelFormat format() const { return f_; }
    int pitch() const { return pitch_; }
    uint8_t* data() { return px_.data(); }
    const uint8_t* data() const { return px_.data(); }
    size_t sizeBytes() const { return px_.size(); }
    uint8_t* row(int y) { return px_.data() + size_t(y) * size_t(pitch_); }
    const uint8_t* row(int y) const { return px_.data() + size_t(y) * size_t(pitch_); }
    uint32_t pixel(int x, int y) const { return readPixel(f_, row(y) + x * bytesPerPixel(f_)); }
    void setPixel(int x, int y, uint32_t raw) { writePixel(f_, row(y) + x * bytesPerPixel(f_), raw); }
    Rect bounds() const { return Rect{0, 0, w_, h_}; }

private:
    int w_ = 0;
    int h_ = 0;
    PixelFormat f_ = PixelFormat::Unknown;
    int pitch_ = 0;
    std::vector<uint8_t> px_;
};

// DDBLT_COLORFILL: raw is a value in dst's format; only its low bytes are written.
void fill(Image& dst, Rect r, uint32_t raw);
// BltFast / Blt of the same size. Both images must have the same format; src and dst may be the same image.
// Every blit returns false, writing nothing, when the source rectangle is not inside the source image
// (DDERR_INVALIDRECT); the destination is clipped.
bool copy(Image& dst, int dx, int dy, const Image& src, Rect sr);
// The same, skipping source pixels whose raw value lies in [keyLow, keyHigh] (source colour key).
bool copyKeyed(Image& dst, int dx, int dy, const Image& src, Rect sr, uint32_t keyLow, uint32_t keyHigh);

enum class StretchFilter { Point, Linear };
// A stretching Blt between images of the same format (mip-level generation, the Bink video).
bool stretch(Image& dst, Rect dr, const Image& src, Rect sr, StretchFilter filter);

}  // namespace uagfx
