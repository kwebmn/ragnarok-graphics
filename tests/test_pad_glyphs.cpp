// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): the controller's buttons drawn on the interface
// - Xbox's coloured letters in rings, PlayStation's shapes, the shoulders' pills with their names, the D-pad's ways
// (core/pad_glyphs)
#include "core/pad_glyphs.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
panel::Picture blankPicture(int w, int h) {
    panel::Picture p;
    p.width = w;
    p.height = h;
    p.px.assign(size_t(w) * size_t(h), 0u);
    return p;
}
uint32_t at(const panel::Picture& p, int x, int y) { return p.px[size_t(y) * size_t(p.width) + size_t(x)]; }
int red(uint32_t c) { return int((c >> 16) & 0xFF); }
int green(uint32_t c) { return int((c >> 8) & 0xFF); }
int blue(uint32_t c) { return int(c & 0xFF); }
}  // namespace

TEST(pad_glyphs_xbox_face_is_a_coloured_ring_with_its_letter) {
    panel::Picture p = blankPicture(32, 32);
    padglyph::paint(p, padglyph::Button::A, padglyph::Family::Xbox, 4, 4, 24);
    const uint32_t ring = at(p, 16, 4);  // its top edge
    CHECK(green(ring) > red(ring) + 40 && green(ring) > blue(ring) + 40);
    const uint32_t inside = at(p, 9, 16);
    CHECK(red(inside) < 40 && green(inside) < 40);  // dark inside
    const padglyph::Letter l = padglyph::letter(padglyph::Button::A, padglyph::Family::Xbox, 4, 4, 24);
    CHECK(l.text == "A" && l.rgb == 0x4CC35Au && l.box.w == 24);
}

TEST(pad_glyphs_sony_face_is_its_shape) {
    panel::Picture p = blankPicture(32, 32);
    padglyph::paint(p, padglyph::Button::A, padglyph::Family::Sony, 4, 4, 24);  // the cross
    const uint32_t middle = at(p, 16, 16);
    CHECK(blue(middle) > 150 && blue(middle) > red(middle));
    CHECK(padglyph::letter(padglyph::Button::A, padglyph::Family::Sony, 4, 4, 24).text.empty());
}

TEST(pad_glyphs_shoulders_are_wider_pills_with_their_names) {
    CHECK(padglyph::widthOf(padglyph::Button::LB, 20) > 28);
    CHECK(padglyph::widthOf(padglyph::Button::A, 20) == 20);
    CHECK(padglyph::letter(padglyph::Button::LB, padglyph::Family::Xbox, 0, 0, 20).text == "LB");
    CHECK(padglyph::letter(padglyph::Button::LB, padglyph::Family::Sony, 0, 0, 20).text == "L1");
    CHECK(padglyph::letter(padglyph::Button::RT, padglyph::Family::Sony, 0, 0, 20).text == "R2");
    CHECK(padglyph::letter(padglyph::Button::Start, padglyph::Family::Sony, 0, 0, 20).text == "Options");
    panel::Picture p = blankPicture(40, 24);
    padglyph::paint(p, padglyph::Button::RB, padglyph::Family::Xbox, 2, 2, 20);
    CHECK(red(at(p, 20, 2)) > 120);  // its light edge on top
}

TEST(pad_glyphs_dpad_points_its_way) {
    panel::Picture p = blankPicture(28, 28);
    padglyph::paint(p, padglyph::Button::Up, padglyph::Family::Xbox, 2, 2, 24);
    int top = 0, bottom = 0;
    for (int y = 6; y < 22; ++y)
        for (int x = 6; x < 22; ++x)
            (y < 14 ? top : bottom) += red(at(p, x, y)) > 150;
    CHECK(top > bottom);
    CHECK(padglyph::buttonOf(1 << 0) == padglyph::Button::A);       // uapad's kPadA
    CHECK(padglyph::buttonOf(1 << 15) == padglyph::Button::RT);     // kPadRT
    CHECK(padglyph::buttonOf(1 << 11) == padglyph::Button::Right);  // kPadRight
}
