#include "core/png_write.hpp"

#include <miniz.h>

#include <cstdio>
#include <cstdlib>

namespace uagfx {

std::vector<uint8_t> pngOf(const uint8_t* bgra, int width, int height, size_t pitch) {
    std::vector<uint8_t> out;
    if (!bgra || width <= 0 || height <= 0) return out;
    std::vector<uint8_t> rgb(size_t(width) * size_t(height) * 3);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const uint8_t* p = bgra + size_t(y) * pitch + size_t(x) * 4;
            uint8_t* q = &rgb[(size_t(y) * size_t(width) + size_t(x)) * 3];
            q[0] = p[2];
            q[1] = p[1];
            q[2] = p[0];
        }
    size_t size = 0;
    void* png = tdefl_write_image_to_png_file_in_memory_ex(rgb.data(), width, height, 3, &size, 6, MZ_FALSE);
    if (!png) return out;
    out.assign(static_cast<uint8_t*>(png), static_cast<uint8_t*>(png) + size);
    mz_free(png);
    return out;
}

bool writePng(const std::string& path, const uint8_t* bgra, int width, int height, size_t pitch) {
    const std::vector<uint8_t> png = pngOf(bgra, width, height, pitch);
    if (png.empty()) return false;
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    return std::fclose(f) == 0 && ok;
}

}  // namespace uagfx
