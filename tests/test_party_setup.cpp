// 3.0.11-beta.158 (task 6 of the remaining windows: the party's settings, Alt+P): the game's «Config Party» window
// (UI window 0x35) as a card (core/party_setup) - its three choices of two (the experience, the things picked up,
// their sharing), each pair the game's own radio buttons (message 6 / 0x95 from the button), OK and cancel its own
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/party_setup.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

TEST(party_setup_the_games_ids) {
    CHECK(partysetup::kChooseMsg == 0x95 && partysetup::kOkButton == 0x76 && partysetup::kCancelButton == 0x77);
    // the choices' values and their radio buttons (the window's ints: the buttons' pointers, then the value)
    CHECK(partysetup::kRows[0].buttons == 0x60 && partysetup::kRows[0].value == 0x68);  // the experience
    CHECK(partysetup::kRows[1].buttons == 0x6C && partysetup::kRows[1].value == 0x74);  // the things picked up
    CHECK(partysetup::kRows[2].buttons == 0x78 && partysetup::kRows[2].value == 0x80);  // their sharing
}

// A card at the middle: three rows of two choices, OK and cancel under them; the mouse on each choice
TEST(party_setup_places_and_the_mouse) {
    const partysetup::Layout L = partysetup::layout(1920, 1080, 1.0f);
    CHECK(L.x + L.w / 2 > 900 && L.x + L.w / 2 < 1020);
    for (int r = 0; r < 3; ++r) CHECK(L.choices[r][1].x > L.choices[r][0].x && L.choices[r][1].y == L.choices[r][0].y);
    CHECK(L.choices[1][0].y > L.choices[0][0].y && L.ok.y > L.choices[2][0].y && L.cancel.y == L.ok.y);
    using P = partysetup::Part;
    const partysetup::Hit h = partysetup::hitTest(L, mid(L.choices[2][1]).first, mid(L.choices[2][1]).second);
    CHECK(h.part == P::Choice && h.row == 2 && h.choice == 1);
    CHECK(partysetup::hitTest(L, mid(L.ok).first, mid(L.ok).second).part == P::Ok);
    CHECK(partysetup::hitTest(L, mid(L.cancel).first, mid(L.cancel).second).part == P::Cancel);
    CHECK(partysetup::hitTest(L, -5, -5).part == P::None);
}

// Its words: the title, each row's name and its two choices, the leader's note when the settings are not one's own;
// its ground painted
TEST(party_setup_words_and_picture) {
    const partysetup::Layout L = partysetup::layout(1920, 1080, 1.0f);
    partysetup::Texts t;
    t.title = "Налаштування групи";
    t.rows[0] = "Досвід";
    t.rows[1] = "Підбір речей";
    t.rows[2] = "Розподіл речей";
    t.choices[0][0] = "Кожному своє";
    t.choices[0][1] = "Порівну";
    t.choices[1][0] = "Кожному своє";
    t.choices[1][1] = "Спільно";
    t.choices[2][0] = "Кожному своє";
    t.choices[2][1] = "Спільно";
    t.ok = "Готово";
    t.cancel = "Скасувати";
    t.leaderOnly = "Змінює лише лідер групи";
    partysetup::Show sh;
    sh.values[0] = 1;
    sh.canChange = false;
    const std::vector<partysetup::Words> w = partysetup::words(L, sh, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const partysetup::Words& x) { return x.text == s; });
    };
    CHECK(has("Налаштування групи") && has("Досвід") && has("Порівну") && has("Спільно") && has("Готово"));
    CHECK(has("Змінює лише лідер групи"));
    sh.canChange = true;
    const std::vector<partysetup::Words> w2 = partysetup::words(L, sh, t);
    CHECK(std::none_of(w2.begin(), w2.end(),
                       [](const partysetup::Words& x) { return x.text == "Змінює лише лідер групи"; }));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    partysetup::paint(p, L, sh);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);
}
