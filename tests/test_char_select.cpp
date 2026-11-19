// 3.0.11-beta.152 (task 3 of the remaining windows, the login's part two): the game's character select in the new
// look (core/char_select) - the page's three places as cards (the face, the name, the class and levels, HP and SP, the
// six stats), the chosen one outlined, an empty place to make one; «Увійти», «Створити», «Видалити», «Назад»; the
// pages
#include <algorithm>
#include <string>
#include <vector>

#include "core/char_select.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

charselect::Texts texts() {
    charselect::Texts t;
    t.title = "Вибір персонажа";
    t.enter = "Увійти";
    t.make = "Створити";
    t.remove = "Видалити";
    t.back = "Назад";
    t.empty = "Вільне місце";
    t.makeHint = "Створіть тут нового персонажа";
    const char* const stats[6] = {"STR", "AGI", "VIT", "INT", "DEX", "LUK"};
    for (int i = 0; i < 6; ++i) t.stats[i] = stats[i];
    return t;
}

charselect::Slot hero() {
    charselect::Slot s;
    s.used = true;
    s.name = "Tester";
    s.job = "Wizard";
    s.level = 99;
    s.jobLevel = 50;
    s.hp = 4200;
    s.maxHp = 5000;
    s.sp = 900;
    s.maxSp = 1200;
    const int stats[6] = {1, 50, 40, 99, 70, 1};
    for (int i = 0; i < 6; ++i) s.stats[i] = stats[i];
    return s;
}

bool has(const std::vector<charselect::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const charselect::Words& x) { return x.text == text; });
}

}  // namespace

// Three places side by side at the screen's middle, the buttons under them, the page's arrows beside the title; the
// mouse on each; the game's own buttons
TEST(char_select_places_and_the_mouse) {
    const charselect::Layout L = charselect::layout(1920, 1080, 1.0f);
    CHECK(L.slots.size() == 3 && L.buttons.size() == 4);
    CHECK(L.slots[1].x > L.slots[0].x + L.slots[0].w && L.slots[2].x > L.slots[1].x + L.slots[1].w);
    CHECK(L.buttons[0].y > L.slots[0].y + L.slots[0].h && std::abs(L.x + L.w / 2 - 960) <= 2);
    CHECK(L.pageRight.x > L.pageLeft.x && L.pageLeft.y < L.slots[0].y);
    const auto mid = [](const panel::Box& b) { return std::pair<float, float>{b.x + b.w / 2, b.y + b.h / 2}; };
    const charselect::Hit h = charselect::hitTest(L, mid(L.slots[2]).first, mid(L.slots[2]).second);
    CHECK(h.part == charselect::Part::Slot && h.index == 2);
    const charselect::Hit b = charselect::hitTest(L, mid(L.buttons[3]).first, mid(L.buttons[3]).second);
    CHECK(b.part == charselect::Part::Button && b.index == 3);
    CHECK(charselect::hitTest(L, mid(L.pageRight).first, mid(L.pageRight).second).part == charselect::Part::PageRight);
    CHECK(charselect::hitTest(L, -5, -5).part == charselect::Part::None);
    using B = charselect::Button;
    CHECK(charselect::gameButton(B::Enter) == 0x76 && charselect::gameButton(B::Make) == 0x89);
    CHECK(charselect::gameButton(B::Remove) == 0x91 && charselect::gameButton(B::Back) == 0x77);
    // the game's 0x96 turns its page on (its message 0x15: the page + 1), 0x97 back (0x14: the page - 1)
    CHECK(charselect::pageButton(true) == 0x96 && charselect::pageButton(false) == 0x97);
}

// Which buttons a place lets press: a character's - enter and delete; an empty one's - make
TEST(char_select_buttons_by_place) {
    using B = charselect::Button;
    charselect::Slot used = hero(), empty;
    CHECK(charselect::allowed(used, B::Enter) && charselect::allowed(used, B::Remove) &&
          !charselect::allowed(used, B::Make));
    CHECK(!charselect::allowed(empty, B::Enter) && !charselect::allowed(empty, B::Remove) &&
          charselect::allowed(empty, B::Make));
    CHECK(charselect::allowed(used, B::Back) && charselect::allowed(empty, B::Back));
}

// Its words: a character's name, class, levels, HP and SP, its stats; an empty place's words; the page; the buttons;
// the chosen one outlined in gold in the picture
TEST(char_select_words_and_picture) {
    const charselect::Layout L = charselect::layout(1920, 1080, 1.0f);
    std::vector<charselect::Slot> slots = {hero(), charselect::Slot{}, charselect::Slot{}};
    charselect::Show sh;
    sh.selected = 0;
    sh.page = 0;
    sh.pages = 3;
    const std::vector<charselect::Words> w = charselect::words(L, slots, sh, texts());
    CHECK(has(w, "Tester") && has(w, "Wizard") && has(w, "Lv 99 / 50") && has(w, "Вільне місце"));
    CHECK(has(w, "HP 4200 / 5000") && has(w, "SP 900 / 1200") && has(w, "INT 99") && has(w, "1 / 3"));
    CHECK(has(w, "Увійти") && has(w, "Створити") && has(w, "Видалити") && has(w, "Назад"));
    ;
}

// 3.0.18-beta.33: uaUI's characters - a card's face and its bars inside the card
TEST(char_select_card_parts_inside_the_card) {
    const panel::Box card{100, 50, 220, 320};
    const charselect::CardParts p = charselect::cardParts(card, 1.0f);
    for (const panel::Box* b : {&p.face, &p.hpBar, &p.spBar})
        CHECK(b->w > 0 && b->x >= card.x && b->x + b->w <= card.x + card.w && b->y >= card.y &&
              b->y + b->h <= card.y + card.h);
    CHECK(p.hpBar.y < p.spBar.y);
}

// 3.0.22-beta.14 (a player's shot through the owner 19.11, «нарисованый интерфейс с проблемами»: the chosen card's
// neighbours wrote their HP, SP and stats over it - uaUI draws every word after every card's box): in the carousel
// nothing of a card lies under a nearer one - the chosen card's neighbours lay their face, bars and words out in the
// part of them it leaves in sight; the outer cards, under nothing, as they were
TEST(char_select_carousel_nothing_under_a_nearer_card) {
    const charselect::Layout L = charselect::carouselLayout(1920, 1080, 1.0f);
    const std::vector<charselect::Slot> slots(5, hero());
    charselect::Show sh;
    sh.selected = charselect::kMiddle;
    sh.page = 7;
    sh.pages = 9;
    const auto over = [](const panel::Box& a, const panel::Box& b) {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
    };
    const int depth[5] = {0, 2, 4, 3, 1};  // 0 and 4 the farthest, then 1 and 3, the chosen 2 the nearest
    const panel::Box& chosen = L.slots[charselect::kMiddle];
    CHECK(over(L.slots[1], chosen) && over(L.slots[3], chosen));  // the neighbours stand partly behind it
    for (const int i : {1, 3}) {
        const panel::Box shown = charselect::shownPart(L, i);
        CHECK(shown.w > 0.6f * L.slots[size_t(i)].w && !over(shown, chosen));
        const charselect::CardParts p = charselect::cardParts(shown, L.s * L.scales[size_t(i)]);
        for (const panel::Box* b : {&p.face, &p.hpBar, &p.spBar}) CHECK(!over(*b, chosen));
    }
    for (const int i : {0, 2, 4}) {
        const panel::Box shown = charselect::shownPart(L, i);
        CHECK(shown.x == L.slots[size_t(i)].x && shown.w == L.slots[size_t(i)].w);
    }
    int others = 0;
    for (const charselect::Words& w : charselect::words(L, slots, sh, texts())) {
        if (w.card < 0) continue;
        others += w.card != charselect::kMiddle;
        for (int j = 0; j < 5; ++j)
            if (depth[j] > depth[w.card]) CHECK(!over(w.box, L.slots[size_t(j)]));
    }
    CHECK(others == 4 * 11);  // each other card's name, class, levels, HP, SP and six stats
}
