#include "core/launcher_art.hpp"

#include <algorithm>
#include <cmath>

#include "core/jpeg_image.hpp"

namespace uagfx::launcherart {

Image decode(const std::string& bytes) {
    Image img;
    if (!jpeg::decode(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), img.argb, img.w, img.h))
        return Image{};
    return img;
}

void cover(const Image& src, int w, int h, std::vector<uint32_t>& out) {
    out.assign(size_t(std::max(w, 0)) * size_t(std::max(h, 0)), kGround);
    if (src.w <= 0 || src.h <= 0 || w <= 0 || h <= 0 || src.argb.size() < size_t(src.w) * size_t(src.h)) return;
    const double k = std::max(double(w) / src.w, double(h) / src.h);  // the larger: it fills both ways
    const double ox = (src.w * k - w) / 2.0, oy = (src.h * k - h) / 2.0;
    std::vector<int> column(size_t(w), 0);
    for (int x = 0; x < w; ++x) column[size_t(x)] = std::clamp(int((x + ox + 0.5) / k), 0, src.w - 1);
    for (int y = 0; y < h; ++y) {
        const int sy = std::clamp(int((y + oy + 0.5) / k), 0, src.h - 1);
        const uint32_t* row = src.argb.data() + size_t(sy) * size_t(src.w);
        uint32_t* to = out.data() + size_t(y) * size_t(w);
        for (int x = 0; x < w; ++x) to[x] = row[column[size_t(x)]];
    }
}

void blend(std::vector<uint32_t>& a, const std::vector<uint32_t>& b, float t) {
    const int k = std::clamp(int(std::lround(t * 256.0f)), 0, 256);
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = a[i], q = b[i];
        uint32_t out = 0xFF000000u;
        for (int shift = 0; shift <= 16; shift += 8) {
            const int x = int((p >> shift) & 255u), y = int((q >> shift) & 255u);
            out |= uint32_t((x * (256 - k) + y * k) >> 8) << shift;
        }
        a[i] = out;
    }
}

}  // namespace uagfx::launcherart
