#pragma once
#include <cstdint>
#include <vector>

#include "core/image.hpp"

namespace uagfx {

class TwinPicture;

// A letter at the twin's size as GetGlyphOutline(GGO_GRAY8_BITMAP) gives it: coverage 0..64 per pixel.
struct GlyphBitmap {
    int width = 0;    // the black box
    int height = 0;
    int originX = 0;  // from the pen to the box's left edge
    int originY = 0;  // from the baseline up to the box's top edge
    std::vector<uint8_t> coverage;  // width x height, row by row
};

// Stage 6 (spec §4): letter i of a TextOutW at game (x, y) goes where the game drew its small one - to the start of
// the block of game pixel x + advance, advance being the width of the letters before it in the game's font.
int letterPen(int x, int advance, double K);
// The baseline: the top of game pixel row y + ascent (the game's font) in the twin.
int baselineRow(int y, int ascent, double K);
// Lays a letter over the twin in colour rgb (0xRRGGBB) with its coverage as alpha, cut to the twin, and reports
// its box as changed.
void drawGlyph(TwinPicture& t, const GlyphBitmap& g, int penX, int baseline, uint32_t rgb);
// The game pixels a TextOutW may touch: its cell box and a margin on every side.
Rect textRect(int x, int y, int extent, int cellHeight, int margin);
// GetTextColor's COLORREF (0x00BBGGRR) as the 0xRRGGBB drawGlyph takes.
uint32_t textColor(uint32_t colorref);

}  // namespace uagfx
