// 3.0.11-beta.216 (the owner: «в интерфейсе нету управления наемниками»): the hired mercenary's block under the
// character block / the party's panel - its kind and level, its name, HP and SP bars, the contract's time left and its
// loyalty; a click opens or closes its card (the game's Ctrl+R window, win/merc_card)
#include <algorithm>
#include <string>
#include <vector>

#include "core/merc_block.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(merc_block_time_left) {
    const std::string m = "ще {1} хв", h = "ще {1} год {2} хв";
    CHECK_EQ(mercblock::leftText(23 * 60 + 10, m, h), std::string("ще 24 хв"));  // a started minute counts
    CHECK_EQ(mercblock::leftText(59, m, h), std::string("ще 1 хв"));
    CHECK_EQ(mercblock::leftText(3600 + 5 * 60, m, h), std::string("ще 1 год 05 хв"));
    CHECK_EQ(mercblock::leftText(0, m, h), std::string());   // ended
    CHECK_EQ(mercblock::leftText(-30, m, h), std::string());
    CHECK(mercblock::ending(4 * 60) && !mercblock::ending(6 * 60) && !mercblock::ending(0));  // under 5 minutes
}

TEST(merc_block_layout_and_hit) {
    const mercblock::Layout L = mercblock::layout(10, 200, 220, 1.0f);
    CHECK(L.x == 10 && L.y == 200 && L.w == 220 && L.h > 60 && L.h < 120);
    CHECK(L.hp.w > 100 && L.sp.w == L.hp.w && L.sp.y > L.hp.y);
    CHECK(mercblock::hit(L, L.glass.x + 5, L.glass.y + 5) && !mercblock::hit(L, -1, 5));
    const mercblock::Layout big = mercblock::layout(10, 200, 0, 1.5f);  // no width given: its own
    CHECK(big.w > 200 && big.h > L.h);
}

TEST(merc_block_picture_and_words) {
    const mercblock::Layout L = mercblock::layout(0, 0, 220, 1.0f);
    mercblock::Info i;
    i.kind = merccard::Kind::Archer;
    i.name = "Rowan";
    i.level = 5;
    i.hp = 300;
    i.maxHp = 1200;
    i.sp = 50;
    i.maxSp = 100;
    i.secondsLeft = 3 * 60;
    i.loyalty = 12;
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    mercblock::Show sh;
    mercblock::paint(p, L, i, sh);
    const auto at = [&](float x, float y) { return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))]; };
    const uint32_t hpLeft = at(L.hp.x + 4, L.hp.y + L.hp.h / 2), hpRight = at(L.hp.x + L.hp.w - 4, L.hp.y + L.hp.h / 2);
    CHECK(((hpLeft >> 8) & 0xFF) > ((hpRight >> 8) & 0xFF) + 40);  // a quarter filled: green on its left only
    mercblock::Texts t;
    t.kinds[0] = "Найманець-лучник";
    t.level = "Рв. {1}";
    t.left = "ще 3 хв";
    t.loyalty = "Вірність";
    const std::vector<mercblock::Words> w = mercblock::words(L, i, t);
    const auto has = [&](const std::string& x) {
        return std::any_of(w.begin(), w.end(), [&](const mercblock::Words& y) { return y.text == x; });
    };
    CHECK(has("Найманець-лучник") && has("Rowan") && has("Рв. 5") && has("ще 3 хв") && has("300 / 1200") &&
          has("Вірність 12"));
    const auto ends = std::find_if(w.begin(), w.end(), [](const mercblock::Words& y) { return y.text == "ще 3 хв"; });
    CHECK(ends != w.end() && ((ends->rgb >> 16) & 0xFF) > ((ends->rgb >> 8) & 0xFF));  // ending: warm
}
