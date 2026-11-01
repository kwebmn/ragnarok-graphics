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
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    charselect::paint(p, L, slots, sh);
    const auto edge = [&](const panel::Box& b) {
        return p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x))];
    };
    const uint32_t gold = edge(L.slots[0]), plain = edge(L.slots[1]);
    CHECK(((gold >> 16) & 0xFF) > ((plain >> 16) & 0xFF) + 40);
}
