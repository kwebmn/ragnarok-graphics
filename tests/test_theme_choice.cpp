// 3.0.24-beta.19 (docs/superpowers/plans/2026-11-27-interface-themes.md, task 4): Alt+O's rows of the interface's look
#include <string>

#include "core/theme_choice.hpp"
#include "core/ui_theme.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(theme_choice_swatch_and_palette) {
    CHECK(themechoice::swatchRow("interface.theme") && !themechoice::swatchRow("interface.theme_hud"));
    CHECK(themechoice::palette().size() == 30u);
    CHECK(themechoice::palette()[18] == uitheme::paletteColour(1, 8));
    const auto pick = themechoice::pick(1, 8);
    CHECK(pick.first == "own" && pick.second.size() == 7 && pick.second[0] == '#');
    CHECK(uitheme::accentOf(pick.first, pick.second) == uitheme::paletteColour(1, 8));
}

TEST(theme_choice_typed_colour_and_dots) {
    CHECK(themechoice::typed("#c86bff") == "#C86BFF" && themechoice::typed("C86BFF") == "#C86BFF");
    CHECK(themechoice::typed(" #C86BFF ") == "#C86BFF");
    CHECK(themechoice::typed("#C86BF") == "" && themechoice::typed("zzz") == "" && themechoice::typed("") == "");
    CHECK(themechoice::typed("#C86BFG") == "" && themechoice::typed("#-12345") == "");
    CHECK(themechoice::dot("auto", "") == "" && themechoice::dot("gold", "") == "#e6cb8a");
    CHECK(themechoice::dot("blue", "") == "#2f5fd0" && themechoice::dot("red", "") == "#c0392b");
    CHECK(themechoice::dot("own", "#C86BFF") == "#c86bff" && themechoice::dot("own", "") == "");
}
