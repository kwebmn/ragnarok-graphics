#include "core/pixel.hpp"

namespace uagfx {
namespace {

// v has `bits` bits -> 8 bits; 0 stays 0 and the maximum becomes 255.
uint32_t expand(uint32_t v, int bits) {
    switch (bits) {
        case 1: return v ? 255u : 0u;
        case 4: return v * 17u;
        case 5: return (v << 3) | (v >> 2);
        case 6: return (v << 2) | (v >> 4);
        default: return v;
    }
}

}  // namespace

int bytesPerPixel(PixelFormat f) {
    switch (f) {
        case PixelFormat::R5G6B5:
        case PixelFormat::X1R5G5B5:
        case PixelFormat::A1R5G5B5:
        case PixelFormat::A4R4G4B4: return 2;
        case PixelFormat::X8R8G8B8:
        case PixelFormat::A8R8G8B8: return 4;
        case PixelFormat::Unknown: break;
    }
    return 0;
}

Masks masksOf(PixelFormat f) {
    switch (f) {
        case PixelFormat::R5G6B5: return {16, 0xF800, 0x07E0, 0x001F, 0};
        case PixelFormat::X1R5G5B5: return {16, 0x7C00, 0x03E0, 0x001F, 0};
        case PixelFormat::A1R5G5B5: return {16, 0x7C00, 0x03E0, 0x001F, 0x8000};
        case PixelFormat::A4R4G4B4: return {16, 0x0F00, 0x00F0, 0x000F, 0xF000};
        case PixelFormat::X8R8G8B8: return {32, 0xFF0000, 0xFF00, 0xFF, 0};
        case PixelFormat::A8R8G8B8: return {32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000};
        case PixelFormat::Unknown: break;
    }
    return {0, 0, 0, 0, 0};
}

PixelFormat formatFromMasks(uint32_t bits, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    const PixelFormat all[] = {PixelFormat::R5G6B5,   PixelFormat::X1R5G5B5, PixelFormat::A1R5G5B5,
                               PixelFormat::A4R4G4B4, PixelFormat::X8R8G8B8, PixelFormat::A8R8G8B8};
    for (PixelFormat f : all) {
        const Masks m = masksOf(f);
        if (m.bits == bits && m.r == r && m.g == g && m.b == b && m.a == a) return f;
    }
    return PixelFormat::Unknown;
}

uint32_t toArgb8(PixelFormat f, uint32_t raw) {
    switch (f) {
        case PixelFormat::R5G6B5:
            return 0xFF000000u | expand((raw >> 11) & 31, 5) << 16 | expand((raw >> 5) & 63, 6) << 8 |
                   expand(raw & 31, 5);
        case PixelFormat::X1R5G5B5:
            return 0xFF000000u | expand((raw >> 10) & 31, 5) << 16 | expand((raw >> 5) & 31, 5) << 8 |
                   expand(raw & 31, 5);
        case PixelFormat::A1R5G5B5:
            return expand((raw >> 15) & 1, 1) << 24 | expand((raw >> 10) & 31, 5) << 16 |
                   expand((raw >> 5) & 31, 5) << 8 | expand(raw & 31, 5);
        case PixelFormat::A4R4G4B4:
            return expand((raw >> 12) & 15, 4) << 24 | expand((raw >> 8) & 15, 4) << 16 |
                   expand((raw >> 4) & 15, 4) << 8 | expand(raw & 15, 4);
        case PixelFormat::X8R8G8B8: return 0xFF000000u | (raw & 0xFFFFFFu);
        case PixelFormat::A8R8G8B8: return raw;
        case PixelFormat::Unknown: break;
    }
    return 0;
}

uint32_t fromArgb8(PixelFormat f, uint32_t argb) {
    const uint32_t a = argb >> 24;
    const uint32_t r = (argb >> 16) & 255;
    const uint32_t g = (argb >> 8) & 255;
    const uint32_t b = argb & 255;
    switch (f) {
        case PixelFormat::R5G6B5: return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
        case PixelFormat::X1R5G5B5: return (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);
        case PixelFormat::A1R5G5B5: return (a >= 128 ? 0x8000u : 0u) | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);
        case PixelFormat::A4R4G4B4: return (a >> 4) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4);
        case PixelFormat::X8R8G8B8: return argb & 0xFFFFFFu;
        case PixelFormat::A8R8G8B8: return argb;
        case PixelFormat::Unknown: break;
    }
    return 0;
}

uint32_t readPixel(PixelFormat f, const uint8_t* p) {
    uint32_t v = 0;
    for (int i = bytesPerPixel(f) - 1; i >= 0; --i) v = v << 8 | p[i];
    return v;
}

void writePixel(PixelFormat f, uint8_t* p, uint32_t raw) {
    const int n = bytesPerPixel(f);
    for (int i = 0; i < n; ++i) p[i] = uint8_t(raw >> (8 * i));
}

void rowToArgb8(PixelFormat f, const uint8_t* src, uint32_t* dst, int count) {
    const int n = bytesPerPixel(f);
    for (int i = 0; i < count; ++i) dst[i] = toArgb8(f, readPixel(f, src + i * n));
}

void rowFromArgb8(PixelFormat f, const uint32_t* src, uint8_t* dst, int count) {
    const int n = bytesPerPixel(f);
    for (int i = 0; i < count; ++i) writePixel(f, dst + i * n, fromArgb8(f, src[i]));
}

uint32_t targetToClient(PixelFormat f, uint32_t argb) {
    const uint32_t r = (argb >> 16) & 255;
    const uint32_t g = (argb >> 8) & 255;
    const uint32_t b = argb & 255;
    switch (f) {
        case PixelFormat::R5G6B5: return (r * 31 / 255) << 11 | (g * 63 / 255) << 5 | (b * 31 / 255);
        case PixelFormat::X1R5G5B5: return (r * 31 / 255) << 10 | (g * 31 / 255) << 5 | (b * 31 / 255);
        default: return fromArgb8(f, argb);
    }
}

}  // namespace uagfx
