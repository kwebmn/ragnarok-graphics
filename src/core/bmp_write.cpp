#include "core/bmp_write.hpp"

#include <cstring>

namespace uagfx::bmp {
namespace {

void put16(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(v >> 8));
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}

}  // namespace

std::vector<uint8_t> write32(const uint32_t* argb, int width, int height) {
    std::vector<uint8_t> b;
    if (!argb || width <= 0 || height <= 0) return b;
    const uint32_t bytes = uint32_t(width) * uint32_t(height) * 4;
    b.reserve(54 + bytes);
    b.push_back('B');
    b.push_back('M');
    put32(b, 54 + bytes);
    put32(b, 0);
    put32(b, 54);
    put32(b, 40);  // BITMAPINFOHEADER
    put32(b, uint32_t(width));
    put32(b, uint32_t(-height));  // top-down
    put16(b, 1);
    put16(b, 32);
    put32(b, 0);  // BI_RGB
    put32(b, bytes);
    put32(b, 2835);
    put32(b, 2835);
    put32(b, 0);
    put32(b, 0);
    const size_t at = b.size();
    b.resize(at + bytes);
    std::memcpy(b.data() + at, argb, bytes);  // 0xAARRGGBB little-endian: B, G, R, A
    return b;
}

}  // namespace uagfx::bmp
