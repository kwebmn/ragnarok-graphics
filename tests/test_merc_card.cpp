// 3.0.11-beta.161 (task 9 of the remaining windows: the mercenary, Ctrl+R): the game's mercenary window (UI window
// 0x84) as a card (core/merc_card) - its kind by its class (archer, spearman, swordsman), name and level, HP and SP
// bars, the contract's end, loyalty, summons and kills, its eight stats, the window's own buttons (skills 0x11b,
// dismiss 0x11c)
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/merc_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

// Its kind as the game's title says it (FUN_00467be0): classes 6017-6026 archers, 6037-6046 spearmen, else swordsmen
TEST(merc_card_kind_by_class) {
    using K = merccard::Kind;
    CHECK(merccard::kindOf(6017) == K::Archer && merccard::kindOf(6026) == K::Archer);
    CHECK(merccard::kindOf(6037) == K::Spearman && merccard::kindOf(6046) == K::Spearman);
    CHECK(merccard::kindOf(6027) == K::Swordsman && merccard::kindOf(6047) == K::Swordsman);
}

TEST(merc_card_buttons_are_the_windows) {
    using B = merccard::Button;
    CHECK(merccard::gameButton(B::Skills) == 0x11b && merccard::gameButton(B::Dismiss) == 0x11c);
    CHECK(merccard::gameButton(B::Close) == 0x87);
}

TEST(merc_card_places_and_the_mouse) {
    const merccard::Layout L = merccard::layout(1920, 1080, 1.0f);
    CHECK(L.x > 960 && L.x + L.w <= 1920);
    CHECK(L.sp.y > L.hp.y && L.facts[3].y > L.sp.y && L.stats[7].y > L.facts[3].y && L.skills.y > L.stats[7].y);
    CHECK(L.dismiss.y > L.skills.y && L.dismiss.w < L.skills.w);
    using B = merccard::Button;
    // 3.0.11-beta.218: «Навички» a list in the card - its heading no button (the game's window lists nothing)
    CHECK(merccard::hitTest(L, mid(L.skills).first, mid(L.skills).second).part == merccard::Part::None);
    CHECK(merccard::hitTest(L, mid(L.dismiss).first, mid(L.dismiss).second).button == B::Dismiss);
    CHECK(merccard::hitTest(L, mid(L.close).first, mid(L.close).second).button == B::Close);
    CHECK(merccard::hitTest(L, -5, -5).part == merccard::Part::None);
}

