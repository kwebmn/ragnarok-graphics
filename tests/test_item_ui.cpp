// 3.0.24-beta.20 (docs/superpowers/plans/2026-11-27-item-windows-old-layout.md, task 1): the item windows' common
// parts - the weight's (and places') bar, the head with its count, each window's keys' hint, the font's letters
#include <string>

#include "core/item_ui.hpp"
#include "core/settings/strings.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(item_ui_fill_tones_and_titles) {
    CHECK(std::string(itemui::fillOf(914, 5810).tone) == "ok" && itemui::fillOf(914, 5810).percent == 15);
    CHECK(std::string(itemui::fillOf(4100, 5810).tone) == "warn");
    CHECK(std::string(itemui::fillOf(5300, 5810).tone) == "full");
    CHECK(itemui::fillOf(1, 0).percent == 0 && std::string(itemui::fillOf(1, 0).tone) == "ok");
    CHECK(itemui::fillOf(9, 5).percent == 100 && std::string(itemui::fillOf(9, 5).tone) == "full");
    CHECK(itemui::countTitle("Речі", 16, 100) == "Речі 16/100");
}

TEST(item_ui_every_window_has_its_hint_in_each_language) {
    for (int w = 0; w < int(itemui::Window::Count); ++w)
        for (settings::Lang l : {settings::Lang::En, settings::Lang::Uk, settings::Lang::Ru}) {
            const std::string hint = settings::tr(l, itemui::hintOf(itemui::Window(w)));
            CHECK(hint.size() > 10);
            CHECK(itemui::fontHas(hint));  // R-WP985Q: no letter the interface's font lacks
        }
}

// R-WP985Q «непонятные символы в карточке»: the font's letters (third_party/plex, src/core/plex_widths.inc)
TEST(item_ui_words_use_the_fonts_letters_only) {
    CHECK(itemui::fontHas("Речі 16/100 · Вага 914 / 5 810 — «обране» №1 1 178 780 z ґЄЇі"));
    CHECK(!itemui::fontHas("\xE2\x98\x85 обране"));       // ★ - a drawn icon, not a letter
    CHECK(!itemui::fontHas("\xE2\x9C\x95"));              // ✕
    CHECK(!itemui::fontHas("\xE2\x96\xB6"));              // ▶
    CHECK(itemui::fontHas("") && !itemui::fontHas("\xFF"));  // nothing; a broken byte
}
