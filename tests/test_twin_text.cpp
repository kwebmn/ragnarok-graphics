#include <utility>
#include <vector>

#include "core/twin.hpp"
#include "core/twin_text.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
uint32_t at(const TwinPicture& t, int x, int y) { return t.twin()[size_t(y) * size_t(t.twinWidth()) + size_t(x)]; }
GlyphBitmap glyph(int w, int h, int ox, int oy, std::vector<uint8_t> coverage) {
    GlyphBitmap g;
    g.width = w;
    g.height = h;
    g.originX = ox;
    g.originY = oy;
    g.coverage = std::move(coverage);
    return g;
}
}  // namespace

TEST(twin_text_letters_start_at_their_game_pixel_blocks) {
    CHECK_EQ(letterPen(10, 0, 1.75), 17);    // floor(17.5)
    CHECK_EQ(letterPen(10, 7, 1.75), 29);    // floor(29.75)
    CHECK_EQ(letterPen(-3, 0, 1.75), -6);    // a list scrolled left
    CHECK_EQ(baselineRow(2, 12, 1.75), 24);  // floor(24.5)
    CHECK_EQ(letterPen(4, 0, 2.0), 8);
}

TEST(twin_text_coverage_becomes_premultiplied_alpha) {
    const std::vector<uint32_t> dib(16, 0xFFFF00FF);  // a transparent picture: a name over the world
    TwinPicture t(dib.data(), 4, 4, 2.0);
    drawGlyph(t, glyph(2, 1, 0, 1, {64, 32}), 1, 3, 0xFFFFFF);  // its box at (1, 2)
    CHECK_EQ(at(t, 1, 2), 0xFFFFFFFFu);
    CHECK_EQ(at(t, 2, 2), 0x80808080u);  // alpha 128, white premultiplied
    CHECK_EQ(at(t, 0, 2), 0u);
    CHECK_EQ(at(t, 1, 1), 0u);
}

TEST(twin_text_letter_over_an_opaque_window) {
    const std::vector<uint32_t> dib(4, 0xFFFFFFFF);
    TwinPicture t(dib.data(), 2, 2, 2.0);
    drawGlyph(t, glyph(1, 1, 0, 0, {32}), 0, 0, 0x000000);  // half-covered black
    CHECK_EQ(at(t, 0, 0), 0xFF7F7F7Fu);
}

TEST(twin_text_letters_are_clipped_to_the_twin) {
    const std::vector<uint32_t> dib(4, 0xFFFFFFFF);
    TwinPicture t(dib.data(), 2, 2, 2.0);  // a 4 x 4 twin
    const GlyphBitmap g = glyph(3, 3, 0, 0, std::vector<uint8_t>(9, 64));
    drawGlyph(t, g, -2, 0, 0x000000);  // its box at (-2, 0): one column inside
    drawGlyph(t, g, 3, 2, 0x000000);   // at (3, 2): one column and two rows inside
    CHECK_EQ(at(t, 0, 0), 0xFF000000u);
    CHECK_EQ(at(t, 1, 0), 0xFFFFFFFFu);
    CHECK_EQ(at(t, 3, 2), 0xFF000000u);
    CHECK_EQ(at(t, 3, 3), 0xFF000000u);
    CHECK_EQ(at(t, 2, 3), 0xFFFFFFFFu);
    drawGlyph(t, glyph(2, 2, 0, 0, {64}), 0, 0, 0x000000);  // a broken bitmap draws nothing
    CHECK_EQ(at(t, 1, 1), 0xFFFFFFFFu);
}

TEST(twin_text_a_letter_reports_its_box) {
    const std::vector<uint32_t> dib(16, 0xFFFFFFFF);
    TwinPicture t(dib.data(), 4, 4, 2.0);  // an 8 x 8 twin
    t.takeChanged();
    drawGlyph(t, glyph(3, 2, 1, 2, std::vector<uint8_t>(6, 64)), 2, 4, 0x000000);  // its box at (3, 2)
    CHECK(t.takeChanged() == (Rect{3, 2, 6, 4}));
    drawGlyph(t, glyph(3, 2, 0, 0, std::vector<uint8_t>(6, 64)), 6, 7, 0x000000);  // at (6, 7): cut
    CHECK(t.takeChanged() == (Rect{6, 7, 8, 8}));
}

TEST(twin_text_rect_has_its_margin) { CHECK(textRect(5, 6, 30, 15, 15) == (Rect{-10, -9, 50, 36})); }

TEST(twin_text_colour_from_gdi) {
    CHECK_EQ(textColor(0x00332211), 0x112233u);  // a COLORREF is 0x00BBGGRR
    CHECK_EQ(textColor(0x02FFFFFF), 0xFFFFFFu);  // its top byte is no colour
}
