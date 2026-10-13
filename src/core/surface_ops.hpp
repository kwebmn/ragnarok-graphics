#pragma once
#include <cstdint>
#include <vector>

#include "core/image.hpp"

namespace uagfx {

// A DirectDraw source colour key (DDSD_CKSRCBLT, SetColorKey): raw values in [low, high] are transparent.
struct ColorKey {
    bool enabled = false;
    uint32_t low = 0;
    uint32_t high = 0;
    bool matches(uint32_t raw) const { return enabled && raw >= low && raw <= high; }
};

// What Lock hands out for a rectangle: the address of its top-left pixel; the pitch stays the image's.
uint8_t* pixelAddress(Image& img, int x, int y);

// r of img as 0xAARRGGBB, row by row; texels matching the key become 0 (alpha 0: the shader discards them).
void toArgbRect(const Image& img, const Rect& r, const ColorKey& key, std::vector<uint32_t>& out);

// Blt / BltFast between CPU surfaces. dr and sr are full rectangles (NULL already replaced by the bounds); sr must
// lie inside src. Equal sizes copy (with the key, matching source texels are skipped); different sizes stretch,
// linearly, or with the nearest texel when keyed so the key colour is not blended into its neighbours. Different
// pixel formats are converted through ARGB8 (the key too: exact for the single-colour keys the client uses).
bool blitCpu(Image& dst, const Rect& dr, const Image& src, const Rect& sr, const ColorKey& key);

// Blt from the render target into a CPU surface. target holds width x height ARGB8 values (Renderer::readTarget).
// Pixels become the surface's format with targetToClient (16-bit floor, calibration.md §5); different sizes take
// the nearest pixel (the calibration readback surface is the target rounded up to a multiple of 4).
bool readTargetInto(Image& dst, const Rect& dr, const std::vector<uint32_t>& target, int width, int height,
                    const Rect& sr);

// Blt from a CPU surface into the render target (width x height): the ARGB8 pixels for dr clipped to the target.
// With a key, matching texels keep the target's pixel from current, the target read back (required then).
struct TargetWrite {
    Rect rect;  // empty when dr lies outside the target: nothing to write
    std::vector<uint32_t> argb;
};
bool targetWrite(const Image& src, const Rect& sr, const Rect& dr, int width, int height, const ColorKey& key,
                 const std::vector<uint32_t>& current, TargetWrite& out);

// The bounds of the pixels whose colour differs (the top byte, which GDI zeroes, is ignored); empty when none.
Rect changedRect(const uint32_t* before, const uint32_t* after, int width, int height);

}  // namespace uagfx
