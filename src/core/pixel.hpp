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

int bytesPerPixel(PixelFormat f);
Masks masksOf(PixelFormat f);
PixelFormat formatFromMasks(uint32_t bits, uint32_t r, uint32_t g, uint32_t b, uint32_t a);

// One pixel: the raw value in format f <-> 0xAARRGGBB with 8-bit channels (alpha 0xFF when f has no alpha).
// Expansion repeats the high bits (5-bit 31 -> 255), so 16-bit values survive the round trip unchanged.
uint32_t toArgb8(PixelFormat f, uint32_t raw);
uint32_t fromArgb8(PixelFormat f, uint32_t argb);
// A render-target pixel (ARGB8 from the GPU) -> the client's screen format when the client reads the target
// back. 16-bit channels are floored from the 8-bit values (v5 = v8 * 31 / 255), so the client's start-up
// calibration settles on (0.5, 0.5) in 16-bit modes too (docs/research/calibration.md §5).
uint32_t targetToClient(PixelFormat f, uint32_t argb);

uint32_t readPixel(PixelFormat f, const uint8_t* p);  // little-endian
void writePixel(PixelFormat f, uint8_t* p, uint32_t raw);
void rowToArgb8(PixelFormat f, const uint8_t* src, uint32_t* dst, int count);
void rowFromArgb8(PixelFormat f, const uint32_t* src, uint8_t* dst, int count);

}  // namespace uagfx
