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
