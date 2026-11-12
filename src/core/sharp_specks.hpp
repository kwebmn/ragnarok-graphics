#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::sharpspecks {

// 3.0.18-beta.181 (the owner: Storm Gust «как в игре, только чётко»; R-8X3RCC's record: its snow two sheets of 64 x 64
// texels, 26-27 dots of 9-15 texels each on black, shown 6-12 times larger on a 4K screen - soft blots): a texture of
// small separate bright dots on black - its dots found and drawn again at real resolution, each one an ellipse of its
// spread with its place, colour and light as the texture had them, its edge sharp. The game still places, colours,
// moves and adds the quads. xBR, tried on the record, turned the dots' outlines ragged and the lightning and the
// ghost's painted strokes into blobs: the dots alone are drawn again.

struct Speck {
    float x = 0, y = 0;               // its middle, texels from the texture's top left
    float sxx = 0, sxy = 0, syy = 0;  // its spread: the light-weighted second moments (a texel's own added)
    float energy = 0, peak = 0;       // its light (texels x their light: the brightest channel times alpha), its most
    float r = 1, g = 1, b = 1;        // its colour, its brightest channel 1
};

// The dots of a sheet of them (ARGB, w x h): none when it is not one - its ground not dark, fewer than eight dots, or a
// glow among them (a cloud, a whirl). Two dots that touch, a dimmer neck between them, are two.
std::vector<Speck> specksOf(const uint32_t* argb, int w, int h);

// The dots drawn `scale` times larger (ARGB, (w * scale) x (h * scale), black and clear between them): each an
// ellipse of its spread as large as its light needs, brightest at its middle, its edge over one pixel of the copy
std::vector<uint32_t> redraw(const std::vector<Speck>& specks, int w, int h, int scale);

// A sheet's copy: 512 on its long side (2-8 times the texture), then its halves down to the texture's own size - the
// card takes the level that fits how large the game shows it. Empty levels: not a sheet of dots.
struct Copy {
    int width = 0, height = 0, scale = 0, specks = 0;
    std::vector<std::vector<uint32_t>> levels;
};
Copy makeCopy(const uint32_t* argb, int w, int h);

}  // namespace uagfx::sharpspecks
