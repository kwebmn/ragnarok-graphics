// 3.0.24-beta.19 (docs/superpowers/plans/2026-11-27-interface-themes.md, task 3): the live kits' own colours - the
// layered windows' (the login, the characters, «Характеристики») boxes and words - through the group's theme in use
#include <array>
#include <string>

#include "core/ui_theme.hpp"
#include "test.hpp"
#include "uikit/layered.hpp"

using namespace uagfx;

TEST(uikit_layered_paints_by_the_group_in_use) {
    uikit::layered::Layer layer;
    {
        uikit::layered::Builder b(layer, panel::Box{0, 0, 100, 100});
        b.word(panel::Box{1, 1, 50, 10}, "dark", 0xEEF2F8, 12, false, uikit::layered::Align::Left);
        b.box(panel::Box{1, 20, 50, 10}, 2, 0x10141C, 1);
    }
    CHECK(!layer.words.empty() && layer.words.back().colour == "#eef2f8ff");  // none set: the dark, as drawn
    uitheme::Look look;
    look.tone = uitheme::Tone::Light;
    std::array<uitheme::Theme, uitheme::kGroups> themes;
    for (int i = 0; i < uitheme::kGroups; ++i) themes[size_t(i)] = uitheme::themeFor(look, uitheme::Group(i));
    uitheme::setActive(themes);
    uikit::layered::Layer light;
    {
        uikit::layered::Builder b(light, panel::Box{0, 0, 100, 100});
        b.word(panel::Box{1, 1, 50, 10}, "light", 0xEEF2F8, 12, false, uikit::layered::Align::Left);
        b.box(panel::Box{1, 20, 50, 10}, 2, 0x10141C, 1);
    }
    uitheme::setActive({});
    CHECK(!light.words.empty() && light.words.back().colour == "#181c26ff");
    CHECK(!light.boxes.empty() && light.boxes.back().fill == "#f6f8fcff");
}
