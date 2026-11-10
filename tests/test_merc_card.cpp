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

TEST(merc_card_words_and_picture) {
    const merccard::Layout L = merccard::layout(1920, 1080, 1.0f);
    merccard::Info i;
    i.kind = merccard::Kind::Archer;
    i.name = "Mina";
    i.level = 40;
    i.hp = 3000;
    i.maxHp = 3500;
    i.sp = 90;
    i.maxSp = 120;
    i.ends = "03.11 21:40";
    i.loyalty = 250;
    i.summons = 12;
    i.kills = 340;
    i.stats[2] = 180;
    i.aspd = 1400;
    merccard::Texts t;
    t.kinds[0] = "Найманець-лучник";
    t.kinds[1] = "Найманець-списник";
    t.kinds[2] = "Найманець-мечник";
    t.level = "Рівень {1}";
    t.facts[0] = "Контракт до";
    t.facts[1] = "Вірність";
    t.facts[2] = "Викликів";
    t.facts[3] = "Убито";
    t.skills = "Навички";
    t.dismiss = "Звільнити";
    const std::vector<merccard::Words> w = merccard::words(L, i, merccard::Show{}, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const merccard::Words& x) { return x.text == s; });
    }
    ;
    CHECK(has("Найманець-лучник") && has("Mina") && has("Рівень 40") && has("3000 / 3500") && has("90 / 120"));
    CHECK(has("03.11 21:40") && has("250") && has("12") && has("340") && has("HIT") && has("180") && has("60"));
    CHECK(has("Звільнити") && has("Навички"));
}

// 3.0.11-beta.218 (R-S8BERR: «блок со скилами наемника пустой»): its skills listed in the card - a row each, the name
// and the level; none known: a dash
TEST(merc_card_lists_its_skills) {
    const merccard::Layout none = merccard::layout(1920, 1080, 1.0f);
    const merccard::Layout L = merccard::layout(1920, 1080, 1.0f, 2);
    CHECK_EQ(L.skillRows.size(), size_t(2));
    CHECK_EQ(none.skillRows.size(), size_t(1));  // the dash's
    CHECK(L.skillRows[0].y > L.skills.y && L.skillRows[1].y > L.skillRows[0].y && L.dismiss.y > L.skillRows[1].y);
    CHECK(L.h > none.h);
    merccard::Info i;
    i.skills = {{"Double Strafe", 2}, {"Berserk", 1}};
    merccard::Texts t;
    t.skills = "Навички";
    t.skillLevel = "Рів. {1}";
    const std::vector<merccard::Words> w = merccard::words(L, i, merccard::Show{}, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const merccard::Words& x) { return x.text == s; });
    };
    CHECK(has("Навички") && has("Double Strafe") && has("Рів. 2") && has("Berserk") && has("Рів. 1"));
    const std::vector<merccard::Words> dash = merccard::words(none, merccard::Info{}, merccard::Show{}, t);
    CHECK(std::any_of(dash.begin(), dash.end(), [](const merccard::Words& x) { return x.text == "\xE2\x80\x94"; }));
}
