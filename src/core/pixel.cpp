#include "core/pixel.hpp"

#include <cstring>

namespace uagfx {
namespace {

// The raw value of pixel i of a row, 16 or 32 bits (memcpy: the game's rows need not be aligned).
inline uint32_t load16(const uint8_t* row, int i) {
    uint16_t v;
    std::memcpy(&v, row + 2 * i, 2);
    return v;
}

inline uint32_t load32(const uint8_t* row, int i) {
    uint32_t v;
    std::memcpy(&v, row + 4 * i, 4);
    return v;
}

inline void store16(uint8_t* row, int i, uint32_t v) {
    const uint16_t s = uint16_t(v);
    std::memcpy(row + 2 * i, &s, 2);
}

inline void store32(uint8_t* row, int i, uint32_t v) { std::memcpy(row + 4 * i, &v, 4); }

// The row's loop for one format, known when compiled: toArgb8's switch goes out of the loop.
template <PixelFormat F, bool Keyed>
void toRow(const uint8_t* src, uint32_t* dst, int count, uint32_t low, uint32_t high) {
    for (int i = 0; i < count; ++i) {
        const uint32_t raw = bytesPerPixel(F) == 4 ? load32(src, i) : load16(src, i);
        dst[i] = Keyed && raw >= low && raw <= high ? 0u : toArgb8(F, raw);
    }
}

template <bool Keyed>
void toRowOf(PixelFormat f, const uint8_t* src, uint32_t* dst, int count, uint32_t low, uint32_t high) {
    switch (f) {
        case PixelFormat::R5G6B5: return toRow<PixelFormat::R5G6B5, Keyed>(src, dst, count, low, high);
        case PixelFormat::X1R5G5B5: return toRow<PixelFormat::X1R5G5B5, Keyed>(src, dst, count, low, high);
        case PixelFormat::A1R5G5B5: return toRow<PixelFormat::A1R5G5B5, Keyed>(src, dst, count, low, high);
        case PixelFormat::A4R4G4B4: return toRow<PixelFormat::A4R4G4B4, Keyed>(src, dst, count, low, high);
        case PixelFormat::X8R8G8B8: return toRow<PixelFormat::X8R8G8B8, Keyed>(src, dst, count, low, high);
        case PixelFormat::A8R8G8B8: return toRow<PixelFormat::A8R8G8B8, Keyed>(src, dst, count, low, high);
        case PixelFormat::Unknown: break;
    }
    for (int i = 0; i < count; ++i) dst[i] = 0;
}

template <PixelFormat F, bool Client>
void fromRow(const uint32_t* src, uint8_t* dst, int count) {
    for (int i = 0; i < count; ++i) {
        const uint32_t raw = Client ? targetToClient(F, src[i]) : fromArgb8(F, src[i]);
        if (bytesPerPixel(F) == 4)
            store32(dst, i, raw);
        else
            store16(dst, i, raw);
    }
}

template <bool Client>
void fromRowOf(PixelFormat f, const uint32_t* src, uint8_t* dst, int count) {
    switch (f) {
        case PixelFormat::R5G6B5: return fromRow<PixelFormat::R5G6B5, Client>(src, dst, count);
        case PixelFormat::X1R5G5B5: return fromRow<PixelFormat::X1R5G5B5, Client>(src, dst, count);
        case PixelFormat::A1R5G5B5: return fromRow<PixelFormat::A1R5G5B5, Client>(src, dst, count);
        case PixelFormat::A4R4G4B4: return fromRow<PixelFormat::A4R4G4B4, Client>(src, dst, count);
        case PixelFormat::X8R8G8B8: return fromRow<PixelFormat::X8R8G8B8, Client>(src, dst, count);
        case PixelFormat::A8R8G8B8: return fromRow<PixelFormat::A8R8G8B8, Client>(src, dst, count);
        case PixelFormat::Unknown: break;
    }
}

}  // namespace

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

void rowToArgb8(PixelFormat f, const uint8_t* src, uint32_t* dst, int count) {
    toRowOf<false>(f, src, dst, count, 0, 0);
}

void rowToArgb8Keyed(PixelFormat f, const uint8_t* src, uint32_t* dst, int count, uint32_t keyLow, uint32_t keyHigh) {
    toRowOf<true>(f, src, dst, count, keyLow, keyHigh);
}

void rowFromArgb8(PixelFormat f, const uint32_t* src, uint8_t* dst, int count) { fromRowOf<false>(f, src, dst, count); }

void rowTargetToClient(PixelFormat f, const uint32_t* src, uint8_t* dst, int count) {
    fromRowOf<true>(f, src, dst, count);
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
