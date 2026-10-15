#include "core/jpeg_image.hpp"

#include <climits>

#include "stb_image.h"

namespace uagfx::jpeg {

bool decode(const uint8_t* data, size_t size, std::vector<uint32_t>& argb, int& width, int& height) {
    if (!data || size == 0 || size > size_t(INT_MAX)) return false;
    int w = 0, h = 0, channels = 0;
    unsigned char* rgb = stbi_load_from_memory(data, int(size), &w, &h, &channels, 3);
    if (!rgb) return false;
    const bool ok = w > 0 && h > 0 && w <= 8192 && h <= 8192;
    if (ok) {
        argb.resize(size_t(w) * size_t(h));
        for (size_t i = 0; i < argb.size(); ++i)
            argb[i] = 0xFF000000u | uint32_t(rgb[3 * i]) << 16 | uint32_t(rgb[3 * i + 1]) << 8 | rgb[3 * i + 2];
        width = w;
        height = h;
    }
    stbi_image_free(rgb);
    return ok;
}

}  // namespace uagfx::jpeg
