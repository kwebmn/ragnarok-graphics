#include "core/ijl.hpp"

#include <climits>
#include <cstdlib>

#include "stb_image.h"
#include "stb_image_write.h"

namespace uagfx::ijl {
namespace {

bool fits(const Dib& d) {
    return d.bytes && d.width > 0 && d.height != 0 && d.padBytes >= 0 && d.channels == 3 &&
           (d.color == kRgb || d.color == kBgr) && d.width <= 16384 && d.height <= 16384 && d.height >= -16384;
}

// The DIB's row of the picture's row y (top first)
uint8_t* rowOf(const Dib& d, int y) {
    const int h = d.height > 0 ? d.height : -d.height;
    const int at = d.height > 0 ? y : h - 1 - y;
    return d.bytes + size_t(at) * size_t(d.width * 3 + d.padBytes);
}

void append(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* b = static_cast<const uint8_t*>(data);
    out->insert(out->end(), b, b + size);
}

}  // namespace

int readParams(const uint8_t* jpg, size_t size, Params& out) {
    int w = 0, h = 0, comp = 0;
    if (!jpg || size == 0 || size > size_t(INT_MAX) || !stbi_info_from_memory(jpg, int(size), &w, &h, &comp))
        return kNotJpeg;
    out.width = w;
    out.height = h;
    out.channels = comp == 1 ? 1 : 3;
    return kOk;
}

int readImage(const uint8_t* jpg, size_t size, const Dib& dib) {
    if (!fits(dib)) return kInvalidProperties;
    Params p;
    if (readParams(jpg, size, p) != kOk) return kNotJpeg;
    const int h = dib.height > 0 ? dib.height : -dib.height;
    if (p.width != dib.width || p.height != h) return kInvalidProperties;
    int w = 0, hh = 0, comp = 0;
    unsigned char* rgb = stbi_load_from_memory(jpg, int(size), &w, &hh, &comp, 3);
    if (!rgb) return kNotJpeg;
    for (int y = 0; y < h; ++y) {
        uint8_t* to = rowOf(dib, y);
        const unsigned char* from = rgb + size_t(y) * size_t(w) * 3;
        for (int x = 0; x < w; ++x, to += 3, from += 3) {
            to[0] = dib.color == kBgr ? from[2] : from[0];
            to[1] = from[1];
            to[2] = dib.color == kBgr ? from[0] : from[2];
        }
    }
    stbi_image_free(rgb);
    return kOk;
}

int writeImage(const Dib& dib, int quality, std::vector<uint8_t>& jpg) {
    if (!fits(dib)) return kInvalidProperties;
    const int h = dib.height > 0 ? dib.height : -dib.height;
    std::vector<uint8_t> rgb(size_t(dib.width) * size_t(h) * 3);  // stb's: RGB, the top row first, unpadded
    for (int y = 0; y < h; ++y) {
        const uint8_t* from = rowOf(dib, y);
        uint8_t* to = &rgb[size_t(y) * size_t(dib.width) * 3];
        for (int x = 0; x < dib.width; ++x, to += 3, from += 3) {
            to[0] = dib.color == kBgr ? from[2] : from[0];
            to[1] = from[1];
            to[2] = dib.color == kBgr ? from[0] : from[2];
        }
    }
    jpg.clear();
    const int q = quality >= 1 && quality <= 100 ? quality : 75;
    return stbi_write_jpg_to_func(append, &jpg, dib.width, h, 3, rgb.data(), q) && !jpg.empty() ? kOk : kMemory;
}

const char* errorText(int code) {
    switch (code) {
        case kOk: return "OK";
        case kMemory: return "out of memory";
        case kInvalidProperties: return "the properties do not fit";
        case kInvalidFilename: return "no such file";
        case kNotJpeg: return "not a JPEG";
        case kFileError: return "the file cannot be read or written";
        case kBufferTooSmall: return "the buffer is too small";
        default: return "an error";
    }
}

}  // namespace uagfx::ijl
