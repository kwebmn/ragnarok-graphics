// 3.0.11-beta.47: the new interface's grounds (core/ui_style) - the concept's opacities and their ranges
#include <cmath>

#include "core/ui_style.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ui_style_opacities) {
    uistyle::set(uistyle::kPanelDefault, uistyle::kWindowDefault);
    CHECK(std::fabs(uistyle::panel() - 0.8f) < 1e-6f && std::fabs(uistyle::window() - 0.9f) < 1e-6f);
    CHECK(std::fabs(uistyle::chat() - 0.8f) < 1e-6f);
    const int was = uistyle::generation();
    uistyle::set(30, 70);  // under their least: the least
    CHECK(std::fabs(uistyle::panel() - 0.5f) < 1e-6f && std::fabs(uistyle::window() - 0.8f) < 1e-6f);
    CHECK(std::fabs(uistyle::chat() - 0.8f) < 1e-6f);  // the chat never under 80 %
    CHECK(uistyle::generation() != was);
    const int now = uistyle::generation();
    uistyle::set(30, 70);  // the same again: nothing to draw again
    CHECK(uistyle::generation() == now);
    uistyle::set(95, 120);
    CHECK(std::fabs(uistyle::panel() - 0.95f) < 1e-6f && std::fabs(uistyle::window() - 1.0f) < 1e-6f);
    CHECK(std::fabs(uistyle::chat() - 0.95f) < 1e-6f);
    uistyle::set(uistyle::kPanelDefault, uistyle::kWindowDefault);
}

// 3.0.11-beta.49: a window's shadow - dark at its edge, fading out, none under the window
TEST(ui_style_window_shadow) {
    const panel::Picture p = uistyle::shadow(100, 60, 12, 16, 0.5f);
    CHECK(p.width == 132 && p.height == 92);
    const auto alpha = [&](int x, int y) { return int(p.px[size_t(y) * size_t(p.width) + size_t(x)] >> 24); };
    CHECK(alpha(66, 46) == 0);                                    // under the window: none
    CHECK(alpha(66, 15) > 100 && alpha(66, 15) <= 128);           // just over its top edge: about the half
    CHECK(alpha(66, 8) > 0 && alpha(66, 8) < alpha(66, 15));      // fading out
    CHECK(alpha(66, 0) == 0 && alpha(0, 0) == 0);                 // its spread's end, the corner's far side
    CHECK((p.px[size_t(15) * size_t(p.width) + 66] & 0xFFFFFF) == 0u);  // black
    CHECK(uistyle::shadow(0, 60, 12, 16, 0.5f).width == 0);
}

// 3.0.11-beta.198 (the owner: «сузить. должно быть акуратно»): the shadow dropped a few pixels - its darkest right
// under the window's bottom edge (no light gap there: beta.49's moved its whole picture down, the empty place under
// the window with it), none under the window itself; the picture `down` taller
TEST(ui_style_window_shadow_dropped) {
    const panel::Picture p = uistyle::shadow(100, 60, 12, 12, 0.4f, 3);
    CHECK(p.width == 124 && p.height == 87);  // 12 round, 3 more under
    const auto alpha = [&](int x, int y) { return int(p.px[size_t(y) * size_t(p.width) + size_t(x)] >> 24); };
    CHECK(alpha(62, 42) == 0);                                  // under the window (12 .. 72 down): none
    CHECK(alpha(62, 72) >= 100 && alpha(62, 74) >= 100);        // right under its bottom edge: the darkest
    CHECK(alpha(62, 80) > 0 && alpha(62, 80) < alpha(62, 74));  // fading out under it
    CHECK(alpha(62, 86) == 0);                                  // its spread's end
    CHECK(alpha(62, 11) < alpha(62, 74));                       // over its top edge: less (dropped)
    CHECK(alpha(0, 0) == 0);
}

// 3.0.11-beta.54 (R-JK5455): the new interface's size - full at 1280 x 720 and up, smaller on a small game, never under
// 60 % nor so small its words fall under 11 of the screen's pixels
TEST(ui_style_size) {
    CHECK(uistyle::autoSize(1920, 1080, 1.0) == 1.0f && uistyle::autoSize(1280, 720, 1.0) == 1.0f);
    CHECK(std::fabs(uistyle::autoSize(896, 672, 2.0) - 0.7f) < 1e-4f);    // R-JK5455's: 896 x 672 in a 1792 window
    CHECK(std::fabs(uistyle::autoSize(1024, 768, 2.0) - 0.8f) < 1e-4f);
    CHECK(std::fabs(uistyle::autoSize(896, 672, 1.0) - 11.0f / 14) < 1e-4f);  // a pixel a pixel: kept legible
    CHECK(std::fabs(uistyle::autoSize(640, 480, 2.0) - 0.6f) < 1e-4f);    // never under 60 %
    CHECK(uistyle::autoSize(0, 0, 1.0) == 1.0f);
    const int was = uistyle::generation();
    uistyle::setSize(0.7f);
    CHECK(std::fabs(uistyle::size() - 0.7f) < 1e-6f && uistyle::generation() != was);
    CHECK(std::fabs(uistyle::partScale(4.0) - 2.8f) < 1e-4f && std::fabs(uistyle::partScale(0.5) - 0.5f) < 1e-4f);
    const int now = uistyle::generation();
    uistyle::setSize(0.7f);  // the same: nothing to draw again
    CHECK(uistyle::generation() == now);
    uistyle::setSize(3.0f);
    CHECK(std::fabs(uistyle::size() - 1.5f) < 1e-6f);
    uistyle::setSize(1.0f);
}

// 3.0.11-beta.67 (the concept: «Масштаб текста — отдельная настройка 80–150 %»): the windows one reads in (the chat,
// «Довідник», «Речі», «Характеристики») at the interface's size times the text's
TEST(ui_style_text_size) {
    uistyle::setSize(1.0f);
    const int was = uistyle::generation();
    uistyle::setText(1.3f);
    CHECK(std::fabs(uistyle::text() - 1.3f) < 1e-6f && uistyle::generation() != was);
    CHECK(std::fabs(uistyle::readingScale(2.0) - 2.6f) < 1e-4f && std::fabs(uistyle::partScale(2.0) - 2.0f) < 1e-4f);
    uistyle::setSize(0.7f);
    CHECK(std::fabs(uistyle::readingScale(2.0) - 1.82f) < 1e-4f);  // both
    const int now = uistyle::generation();
    uistyle::setText(1.3f);  // the same: nothing to draw again
    CHECK(uistyle::generation() == now);
    uistyle::setText(0.2f);
    CHECK(std::fabs(uistyle::text() - 0.8f) < 1e-6f);  // 80 % at the least, 150 at the most
    uistyle::setText(9.0f);
    CHECK(std::fabs(uistyle::text() - 1.5f) < 1e-6f);
    uistyle::setText(1.0f);
    uistyle::setSize(1.0f);
}

// 3.0.11-beta.74 (the owner: «иконки ... грузить в текстур которые обработаны»): the panels' pictures' source (the AI
// pack read, [interface] pack_icons) - another one draws the parts again, the same one nothing
TEST(ui_style_pictures_source) {
    uistyle::setPictures(-1);
    const int g = uistyle::generation();
    uistyle::setPictures(-1);
    CHECK_EQ(uistyle::generation(), g);
    uistyle::setPictures(1);
    CHECK(uistyle::generation() != g);
    const int h = uistyle::generation();
    uistyle::setPictures(1);
    CHECK_EQ(uistyle::generation(), h);
    uistyle::setPictures(-1);
}
