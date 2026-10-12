#include <cstdint>

#include "core/pixel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
const PixelFormat kAll[] = {PixelFormat::R5G6B5, PixelFormat::X1R5G5B5, PixelFormat::A1R5G5B5,
                            PixelFormat::A4R4G4B4, PixelFormat::X8R8G8B8, PixelFormat::A8R8G8B8};
}

TEST(pixel_sizes) {
    CHECK_EQ(bytesPerPixel(PixelFormat::R5G6B5), 2);
    CHECK_EQ(bytesPerPixel(PixelFormat::A4R4G4B4), 2);
    CHECK_EQ(bytesPerPixel(PixelFormat::A8R8G8B8), 4);
    CHECK_EQ(bytesPerPixel(PixelFormat::Unknown), 0);
}

TEST(pixel_masks_round_trip) {
    for (PixelFormat f : kAll) {
        const Masks m = masksOf(f);
        CHECK(formatFromMasks(m.bits, m.r, m.g, m.b, m.a) == f);
    }
    CHECK(formatFromMasks(16, 0xF800, 0x07E0, 0x001F, 0x0001) == PixelFormat::Unknown);
    CHECK(formatFromMasks(24, 0xFF0000, 0xFF00, 0xFF, 0) == PixelFormat::Unknown);
}

TEST(pixel_expansion_reaches_full_range) {
    CHECK_EQ(toArgb8(PixelFormat::R5G6B5, 0xFFFF), 0xFFFFFFFFu);
    CHECK_EQ(toArgb8(PixelFormat::R5G6B5, 0x0000), 0xFF000000u);
    CHECK_EQ(toArgb8(PixelFormat::R5G6B5, 0xF800), 0xFFFF0000u);
    CHECK_EQ(toArgb8(PixelFormat::R5G6B5, 0x07E0), 0xFF00FF00u);
    CHECK_EQ(toArgb8(PixelFormat::X1R5G5B5, 0x7FFF), 0xFFFFFFFFu);
}

TEST(pixel_alpha_formats) {
    CHECK_EQ(toArgb8(PixelFormat::A1R5G5B5, 0x7C00), 0x00FF0000u);  // red, transparent (magenta turned into alpha)
    CHECK_EQ(toArgb8(PixelFormat::A1R5G5B5, 0xFC00), 0xFFFF0000u);
    CHECK_EQ(toArgb8(PixelFormat::A4R4G4B4, 0xF0F0), 0xFF00FF00u);
    CHECK_EQ(toArgb8(PixelFormat::A4R4G4B4, 0x8421), 0x88442211u);
    CHECK_EQ(toArgb8(PixelFormat::X8R8G8B8, 0x00123456), 0xFF123456u);
    CHECK_EQ(toArgb8(PixelFormat::A8R8G8B8, 0x80123456), 0x80123456u);
}

TEST(pixel_16bit_round_trip_is_lossless) {
    for (uint32_t v = 0; v < 0x10000; ++v) {
        CHECK_EQ(fromArgb8(PixelFormat::R5G6B5, toArgb8(PixelFormat::R5G6B5, v)), v);
        CHECK_EQ(fromArgb8(PixelFormat::A1R5G5B5, toArgb8(PixelFormat::A1R5G5B5, v)), v);
        CHECK_EQ(fromArgb8(PixelFormat::A4R4G4B4, toArgb8(PixelFormat::A4R4G4B4, v)), v);
        CHECK_EQ(fromArgb8(PixelFormat::X1R5G5B5, toArgb8(PixelFormat::X1R5G5B5, v)), v & 0x7FFF);
    }
}

TEST(pixel_rows_are_little_endian) {
    const uint8_t src[4] = {0x00, 0xF8, 0xE0, 0x07};  // R5G6B5: red, green
    uint32_t argb[2] = {};
    rowToArgb8(PixelFormat::R5G6B5, src, argb, 2);
    CHECK_EQ(argb[0], 0xFFFF0000u);
    CHECK_EQ(argb[1], 0xFF00FF00u);
    uint8_t back[4] = {};
    rowFromArgb8(PixelFormat::R5G6B5, argb, back, 2);
    CHECK(back[0] == 0x00 && back[1] == 0xF8 && back[2] == 0xE0 && back[3] == 0x07);
    uint8_t p[4] = {};
    writePixel(PixelFormat::A8R8G8B8, p, 0x11223344);
    CHECK(p[0] == 0x44 && p[3] == 0x11);
    CHECK_EQ(readPixel(PixelFormat::A8R8G8B8, p), 0x11223344u);
}
