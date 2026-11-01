// 3.0.11-beta.140 (the owner «давай» to the first of the game's windows not redrawn yet): the game's menu and its death
// screen (core/menu_window) - which buttons each of its three windows has and the game's own ids for them, the card's
// places, the mouse over it, the safe button first in focus, its picture and words
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/menu_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

menuwin::Texts texts() {
    menuwin::Texts t;
    t.menu = "Меню";
    t.dead = "Ви загинули";
    t.revive = "Воскреснути тут (жетон Зігфріда)";
    t.savePoint = "До точки збереження";
    t.charSelect = "Вибір персонажа";
    t.exit = "Вийти з гри";
    t.back = "Повернутися до гри";
    t.enter = "Enter";
    t.esc = "Esc";
    return t;
}

// A letter 0.6 of its height wide, as the game's bold Cyrillic about (UTF-8: its first byte counted)
float measure(const std::string& s, float px, bool) {
    return float(std::count_if(s.begin(), s.end(), [](char c) { return (uint8_t(c) & 0xC0) != 0x80; })) * px * 0.6f;
}

}  // namespace

// Alive (Esc: UI window 0x30) - character select 0x9A, exit 0x9B, back 0x77; dead (0x13) - the save point 0x9A first,
// character select 0xBA, exit 0xB9, back 0x77; dead with a Token of Siegfried in the bag (0x88) - revive here 0x11F
// first
TEST(menu_window_buttons_of_each_window) {
    using A = menuwin::Action;
    CHECK((menuwin::actions(menuwin::Kind::Alive) == std::vector<A>{A::CharSelect, A::Exit, A::Back}));
    CHECK((menuwin::actions(menuwin::Kind::Dead) == std::vector<A>{A::SavePoint, A::CharSelect, A::Exit, A::Back}));
    CHECK((menuwin::actions(menuwin::Kind::DeadToken) ==
           std::vector<A>{A::Revive, A::SavePoint, A::CharSelect, A::Exit, A::Back}));
    CHECK(menuwin::buttonId(menuwin::Kind::Alive, A::CharSelect) == 0x9A);
    CHECK(menuwin::buttonId(menuwin::Kind::Alive, A::Exit) == 0x9B);
    CHECK(menuwin::buttonId(menuwin::Kind::Alive, A::Back) == 0x77);
    CHECK(menuwin::buttonId(menuwin::Kind::Dead, A::SavePoint) == 0x9A);
    CHECK(menuwin::buttonId(menuwin::Kind::Dead, A::CharSelect) == 0xBA);
    CHECK(menuwin::buttonId(menuwin::Kind::Dead, A::Exit) == 0xB9);
    CHECK(menuwin::buttonId(menuwin::Kind::DeadToken, A::Revive) == 0x11F);
    CHECK(menuwin::buttonId(menuwin::Kind::Alive, A::SavePoint) == 0);  // not in that window
}

// A card in the middle across, under the middle of the height (the character stays seen above it); a row a button;
// the last («Повернутися до гри») in focus at first - Enter by chance changes nothing
TEST(menu_window_places_and_the_mouse) {
    const menuwin::Layout L = menuwin::layout(menuwin::Kind::Dead, 1920, 1080, 1.0f);
    CHECK(L.buttons.size() == 4);
    CHECK(L.x + L.w / 2 > 900 && L.x + L.w / 2 < 1020);  // centred across
    CHECK(L.y > 1080 / 2 - 60 && L.y + L.h < 1080);       // under the middle
    for (size_t i = 1; i < L.buttons.size(); ++i) CHECK(L.buttons[i].y > L.buttons[i - 1].y + L.buttons[i - 1].h - 1);
    const panel::Box& b = L.buttons[2];
    CHECK(menuwin::buttonAt(L, b.x + b.w / 2, b.y + b.h / 2) == 2);
    CHECK(menuwin::buttonAt(L, -5, -5) == -1);
    CHECK(menuwin::firstFocus(menuwin::Kind::Dead) == 3 && menuwin::firstFocus(menuwin::Kind::Alive) == 2);
    CHECK(menuwin::layout(menuwin::Kind::Alive, 1920, 1080, 1.0f).buttons.size() == 3);
    CHECK(menuwin::layout(menuwin::Kind::DeadToken, 1920, 1080, 1.0f).h > L.h);
}

// Its words: the title (dead: «Ви загинули» in red), each button's, the focused one's key on its right (Enter), Esc on
// «Повернутися до гри»; its ground painted
TEST(menu_window_picture_and_words) {
    const menuwin::Texts t = texts();
    const menuwin::Layout L = menuwin::layout(menuwin::Kind::Dead, 1920, 1080, 1.0f);
    menuwin::Show sh;
    sh.focus = 0;
    const std::vector<menuwin::Words> w = menuwin::words(L, menuwin::Kind::Dead, sh, t, measure);
    const auto find = [&](const std::string& s) {
        return std::find_if(w.begin(), w.end(), [&](const menuwin::Words& x) { return x.text == s; });
    };
    CHECK(find("Ви загинули") != w.end() && find("Ви загинули")->rgb == 0xFF5A4E);
    CHECK(find("До точки збереження") != w.end() && find("Вийти з гри") != w.end());
    CHECK(find("Enter") != w.end() && find("Esc") != w.end());
    if (find("Enter") != w.end())  // on the focused row (the picture's pixels, as the buttons)
        CHECK(find("Enter")->box.y >= L.buttons[0].y - 1 && find("Enter")->box.y < L.buttons[0].y + L.buttons[0].h);
    const std::vector<menuwin::Words> alive = menuwin::words(menuwin::layout(menuwin::Kind::Alive, 1920, 1080, 1.0f),
                                                             menuwin::Kind::Alive, menuwin::Show{}, t, measure);
    CHECK(std::any_of(alive.begin(), alive.end(), [](const menuwin::Words& x) { return x.text == "Меню"; }));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    menuwin::paint(p, L, menuwin::Kind::Dead, sh, t);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);  // the card's ground
}

// A row's words longer than its room beside its key («Воскреснути тут (жетон Зігфріда)» at 1.0): the key left out, the
// words whole (the game's text cut at the end otherwise)
TEST(menu_window_long_words_drop_the_key) {
    const menuwin::Texts t = texts();
    const menuwin::Layout L = menuwin::layout(menuwin::Kind::DeadToken, 1920, 1080, 1.0f);
    menuwin::Show sh;
    sh.focus = 0;
    const std::vector<menuwin::Words> w = menuwin::words(L, menuwin::Kind::DeadToken, sh, t, measure);
    CHECK(std::none_of(w.begin(), w.end(), [](const menuwin::Words& x) { return x.text == "Enter"; }));
    const auto revive = std::find_if(w.begin(), w.end(), [&](const menuwin::Words& x) { return x.text == t.revive; });
    CHECK(revive != w.end() && revive->box.w >= L.buttons[0].w - 28.5f);
    sh.focus = 1;  // «До точки збереження» fits beside it
    const std::vector<menuwin::Words> w2 = menuwin::words(L, menuwin::Kind::DeadToken, sh, t, measure);
    CHECK(std::any_of(w2.begin(), w2.end(), [](const menuwin::Words& x) { return x.text == "Enter"; }));
}
