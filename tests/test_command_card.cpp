// 3.0.11-beta.206 (the tester's third list: «alt j - command list в старому стилі» - the game's command list, Alt+Y):
// the game's command window (UI window 0x70) as a card (core/command_card) - its three lists (the switches with their
// state, the commands, the windows' Alt keys) on tabs, two columns
#include <algorithm>
#include <string>
#include <vector>

#include "core/command_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

commandcard::Texts texts() {
    commandcard::Texts t;
    t.title = "Список команд";
    t.tabs[0] = "Перемикачі";
    t.tabs[1] = "Команди";
    t.tabs[2] = "Вікна";
    t.on = "увімк.";
    t.off = "вимк.";
    return t;
}

std::vector<commandcard::Entry> entries(int n) {
    std::vector<commandcard::Entry> e;
    for (int i = 0; i < n; ++i) e.push_back(commandcard::Entry{"/c" + std::to_string(i), 0x10 + i, -1});
    return e;
}

bool has(const std::vector<commandcard::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const commandcard::Words& x) { return x.text == text; });
}

}  // namespace

// The windows' keys as the game names them: Alt and the letter
TEST(command_card_key_names) {
    CHECK(commandcard::keyName(0x43) == "Alt+C");
    CHECK(commandcard::keyName(0x4A) == "Alt+J");
    CHECK(commandcard::keyName(0x10).empty());
}

// The toggles' states by their codes (the game's list of pairs {state, code}); not a toggle -1
TEST(command_card_states_by_code) {
    std::vector<commandcard::Entry> e = entries(3);
    commandcard::markStates(e, {{1, 0x11}, {0, 0x12}, {1, 0x99}});
    CHECK_EQ(e[0].state, -1);
    CHECK_EQ(e[1].state, 1);
    CHECK_EQ(e[2].state, 0);
}

// Two columns, as many rows as the longest list needs; the tabs over them; each part found where it is drawn
TEST(command_card_layout_and_hits) {
    const commandcard::Layout L = commandcard::layout(23, 1920, 1080, 1.0f);
    CHECK_EQ(int(L.rows.size()), 24);  // 12 rows of two
    CHECK_EQ(int(L.tabs.size()), 3);
    CHECK_EQ(L.x, 1920 / 2 - L.w / 2);
    CHECK(L.y >= 0 && L.y + L.h <= 1080);
    CHECK(L.rows[0].y == L.rows[1].y && L.rows[0].x < L.rows[1].x && L.rows[2].y > L.rows[0].y);
    CHECK(L.tabs[0].y + L.tabs[0].h <= L.rows[0].y);
    using P = commandcard::Part;
    const auto mid = [](const panel::Box& b) { return std::pair<float, float>{b.x + b.w / 2, b.y + b.h / 2}; };
    commandcard::Hit h = commandcard::hitTest(L, mid(L.rows[5]).first, mid(L.rows[5]).second);
    CHECK(h.part == P::Row && h.index == 5);
    h = commandcard::hitTest(L, mid(L.tabs[2]).first, mid(L.tabs[2]).second);
    CHECK(h.part == P::Tab && h.index == 2);
    h = commandcard::hitTest(L, mid(L.close).first, mid(L.close).second);
    CHECK(h.part == P::Close);
    h = commandcard::hitTest(L, 2, 2);
    CHECK(h.part == P::None);
    const commandcard::Layout small = commandcard::layout(23, 800, 600, 0.8f);
    CHECK(small.y >= 0 && small.y + small.h <= 600);
}

