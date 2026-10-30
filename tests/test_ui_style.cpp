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
