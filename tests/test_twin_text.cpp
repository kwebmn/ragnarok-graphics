#include <algorithm>
#include <cstdlib>
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

TEST(twin_text_letters_centred_in_their_game_places_and_stay_as_text_grows) {
    // "Command" in the game's hinted 15 px Arial (m 13 px wide) and its large letters at k 1.75 (m 19 px).
    const std::vector<int> ends = {10, 18, 31, 44, 51, 59, 67};
    const std::vector<int> advances = {16, 13, 19, 19, 12, 13, 13};
    const double K = 1.75;
    const std::vector<int> pens = letterPens(100, ends, advances, K);
    CHECK_EQ(int(pens.size()), 7);
    for (size_t i = 0; i < pens.size(); ++i) {  // each within its own game place, in its middle
        const int from = letterPen(100, i ? ends[i - 1] : 0, K), to = letterPen(100, ends[i], K);
        const int before = pens[i] - from, after = to - (pens[i] + advances[i]);
        CHECK(std::abs(before - after) <= 1);
    }
    // 1.3.4: typed letter by letter, the ones already there do not move (the login's and the chat's field).
    for (size_t k = 1; k <= ends.size(); ++k) {
        const std::vector<int> part = letterPens(100, std::vector<int>(ends.begin(), ends.begin() + long(k)),
                                                 std::vector<int>(advances.begin(), advances.begin() + long(k)), K);
        for (size_t i = 0; i < k; ++i) CHECK_EQ(part[i], pens[i]);
    }
    // The gap after the m's split either side of it, not all after it (letterPen left 4 px after it).
    const int mFrom = letterPen(100, ends[2], K), mTo = letterPen(100, ends[3], K);
    CHECK(pens[3] - mFrom >= 1 && mTo - (pens[3] + advances[3]) <= 3);
    CHECK(letterPens(0, {}, {}, K).empty());
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

TEST(twin_text_a_label_redrawn_over_its_skin_stays_the_same) {
    // A skin button: the game repaints its skin with CopyPixels, then draws its label. The twin must look the same
    // after every redraw, not bolder (review 0.5.0 I1).
    const std::vector<uint32_t> skin(6 * 3, 0xFFC0C0C0);
    const std::vector<uint32_t> dib = skin;
    TwinPicture t(dib.data(), 6, 3, 1.75);
    const GlyphBitmap letter = glyph(4, 3, 0, 3, {16, 48, 48, 16, 32, 64, 64, 32, 16, 48, 48, 16});
    std::vector<uint32_t> once;
    for (int i = 0; i < 10; ++i) {
        t.copyPixels(dib.data(), 0, 0, 6, 3, skin.data(), false);
        drawGlyph(t, letter, 3, 4, 0x000000);
        if (i == 0) once = t.twin();
    }
    CHECK(t.twin() == once);
}

TEST(twin_text_font_fits_the_game_letters) {
    // The game's hinted 15-pixel Arial is narrower than the 26-pixel one over 1.75: the twin's font steps down until
    // the sample is no wider than the game's (0.5.1: letters touched and stuck out of their frames).
    const auto width = [](int h) { return h == 26 ? 1264 : h == 25 ? 1192 : h * 45; };
    CHECK_EQ(fitFontHeight(26, 21, 1233.75, width), 25);
    CHECK_EQ(fitFontHeight(26, 21, 1300.0, width), 26);  // already fits: as large as K times the game's
}

TEST(twin_text_font_fit_has_a_floor_and_falls_back) {
    const auto wide = [](int) { return 5000; };
    CHECK_EQ(fitFontHeight(26, 21, 1000.0, wide), 21);  // never below the floor
    const auto unknown = [](int) { return 0; };
    CHECK_EQ(fitFontHeight(26, 21, 1000.0, unknown), 26);  // nothing measures: K times the game's
    CHECK_EQ(fitFontHeight(26, 21, 0.0, wide), 26);        // the game's width unknown
    CHECK_EQ(fitFontHeight(0, 0, 1000.0, wide), 0);        // the game's default height stays default
}
