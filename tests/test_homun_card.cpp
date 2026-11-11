// 3.0.11-beta.160 (task 9 of the remaining windows: the homunculus, Alt+R): the game's homunculus window (UI window
// 0x71) as a card (core/homun_card) - its name and level, HP, SP and experience bars, fullness and closeness (the
// game's eight closeness words), its eight stats, the window's own buttons (feed 0xe0, its skills 0xdf, delete 0xe1)
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/homun_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

// The game's closeness words by its thresholds (FUN_00466be0): 3 / 10 / 100 / 250 / 750 / 910 / 1000, past it
// «unknown»; the attack speed shown as the game shows it ((2000 - its delay) / 10)
TEST(homun_card_closeness_and_speed) {
    CHECK(homuncard::intimacyState(0) == 0 && homuncard::intimacyState(3) == 0 && homuncard::intimacyState(4) == 1);
    using homuncard::intimacyState;
    CHECK(intimacyState(10) == 1 && intimacyState(11) == 2 && intimacyState(100) == 2);
    CHECK(intimacyState(101) == 3 && intimacyState(250) == 3 && intimacyState(251) == 4);
    CHECK(intimacyState(750) == 4 && intimacyState(751) == 5 && intimacyState(910) == 5);
    CHECK(intimacyState(911) == 6 && intimacyState(1000) == 6 && intimacyState(1001) == 7);
    CHECK(homuncard::aspd(1500) == 50 && homuncard::aspd(400) == 160);
}

TEST(homun_card_buttons_are_the_windows) {
    using B = homuncard::Button;
    CHECK(homuncard::gameButton(B::Feed) == 0xe0 && homuncard::gameButton(B::Skills) == 0xdf);
    CHECK(homuncard::gameButton(B::Delete) == 0xe1 && homuncard::gameButton(B::Close) == 0x87);
}

// A card on the right: the title and ✕, the bars, the stats in two columns, the feed and skills buttons, the delete
// link small at the foot; the mouse on each
TEST(homun_card_places_and_the_mouse) {
    const homuncard::Layout L = homuncard::layout(1920, 1080, 1.0f);
    CHECK(L.x > 960 && L.x + L.w <= 1920);
    CHECK(L.sp.y > L.hp.y && L.exp.y > L.sp.y && L.stats[7].y > L.exp.y && L.buttons[0].y > L.stats[7].y);
    CHECK(L.remove.y > L.buttons[0].y && L.remove.w < L.buttons[0].w);
    using P = homuncard::Part;
    using B = homuncard::Button;
    // 3.0.11-beta.221: its skills listed in the card - no button for the game's window (it named none of them)
    CHECK(L.buttons[1].w == 0 && L.buttons[0].w > 200);
    CHECK(homuncard::hitTest(L, mid(L.remove).first, mid(L.remove).second).button == B::Delete);
    CHECK(homuncard::hitTest(L, mid(L.close).first, mid(L.close).second).button == B::Close);
    CHECK(homuncard::hitTest(L, -5, -5).part == P::None);
}

