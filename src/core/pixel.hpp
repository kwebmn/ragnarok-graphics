#pragma once
#include <cstdint>

namespace uagfx {

// The pixel formats of DirectDraw surfaces the client can see. Textures are A1R5G5B5 or A4R4G4B4;
// the screen is R5G6B5, X1R5G5B5 or X8R8G8B8.
enum class PixelFormat : uint8_t { Unknown, R5G6B5, X1R5G5B5, A1R5G5B5, A4R4G4B4, X8R8G8B8, A8R8G8B8 };

struct Masks {
    uint32_t bits;
    uint32_t r, g, b, a;
};

Masks masksOf(PixelFormat f);
PixelFormat formatFromMasks(uint32_t bits, uint32_t r, uint32_t g, uint32_t b, uint32_t a);

// 2.5.11: the functions of one pixel are inline - the loops over the game's textures and frames call them for each
// pixel, and as calls into another file they were a fifth of the game's thread with Direct3D 11.
inline int bytesPerPixel(PixelFormat f) {
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

namespace detail {

// v has 5, 6 or 4 bits -> 8 bits; 0 stays 0 and the maximum becomes 255.
constexpr uint32_t expand5(uint32_t v) { return (v << 3) | (v >> 2); }
constexpr uint32_t expand6(uint32_t v) { return (v << 2) | (v >> 4); }
constexpr uint32_t expand4(uint32_t v) { return v * 17u; }

}  // namespace detail

// One pixel: the raw value in format f <-> 0xAARRGGBB with 8-bit channels (alpha 0xFF when f has no alpha).
// Expansion repeats the high bits (5-bit 31 -> 255), so 16-bit values survive the round trip unchanged.
inline uint32_t toArgb8(PixelFormat f, uint32_t raw) {
    using namespace detail;
    switch (f) {
        case PixelFormat::R5G6B5:
            return 0xFF000000u | expand5((raw >> 11) & 31) << 16 | expand6((raw >> 5) & 63) << 8 | expand5(raw & 31);
        case PixelFormat::X1R5G5B5:
            return 0xFF000000u | expand5((raw >> 10) & 31) << 16 | expand5((raw >> 5) & 31) << 8 | expand5(raw & 31);
        case PixelFormat::A1R5G5B5:
            return ((raw >> 15) & 1 ? 0xFF000000u : 0u) | expand5((raw >> 10) & 31) << 16 |
                   expand5((raw >> 5) & 31) << 8 | expand5(raw & 31);
        case PixelFormat::A4R4G4B4:
            return expand4((raw >> 12) & 15) << 24 | expand4((raw >> 8) & 15) << 16 | expand4((raw >> 4) & 15) << 8 |
                   expand4(raw & 15);
        case PixelFormat::X8R8G8B8: return 0xFF000000u | (raw & 0xFFFFFFu);
        case PixelFormat::A8R8G8B8: return raw;
        case PixelFormat::Unknown: break;
    }
    return 0;
}

inline uint32_t fromArgb8(PixelFormat f, uint32_t argb) {
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

// A render-target pixel (ARGB8 from the GPU) -> the client's screen format when the client reads the target
// back. 16-bit channels are floored from the 8-bit values (v5 = v8 * 31 / 255), so the client's start-up
// calibration settles on (0.5, 0.5) in 16-bit modes too (docs/research/calibration.md §5).
uint32_t targetToClient(PixelFormat f, uint32_t argb);

inline uint32_t readPixel(PixelFormat f, const uint8_t* p) {  // little-endian
    switch (bytesPerPixel(f)) {
        case 2: return uint32_t(p[0]) | uint32_t(p[1]) << 8;
        case 4: return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
        default: return 0;
    }
}

inline void writePixel(PixelFormat f, uint8_t* p, uint32_t raw) {
    const int n = bytesPerPixel(f);
    for (int i = 0; i < n; ++i) p[i] = uint8_t(raw >> (8 * i));
}

// A row of `count` pixels, the format chosen once for it (2.5.11).
void rowToArgb8(PixelFormat f, const uint8_t* src, uint32_t* dst, int count);
// ... the texels whose raw value lies in [keyLow, keyHigh] (a colour key) become 0: alpha 0, the shader drops them
void rowToArgb8Keyed(PixelFormat f, const uint8_t* src, uint32_t* dst, int count, uint32_t keyLow, uint32_t keyHigh);
void rowFromArgb8(PixelFormat f, const uint32_t* src, uint8_t* dst, int count);
void rowTargetToClient(PixelFormat f, const uint32_t* src, uint8_t* dst, int count);  // targetToClient's

}  // namespace uagfx
