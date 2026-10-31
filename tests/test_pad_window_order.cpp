// 3.0.11-beta.93 (R-KMG8GE: B pressed on the game's equipment and options windows and on the big map went to the empty
// slot F3, the big map closed only with the mouse): the windows B closes, in the order they opened (pad core
// window_order), and the hint line while one is open
#include "pad/core/logic/npc_hint.hpp"
#include "pad/core/logic/window_order.hpp"
#include "test.hpp"

using namespace uapad;

TEST(window_order_b_closes_the_last_opened) {
    WindowOrder o;
    CHECK_EQ(o.last(), -1);
    o.see(1, true);
    o.see(3, true);
    CHECK_EQ(o.last(), 3);
    o.see(3, false);  // closed with the mouse
    CHECK_EQ(o.last(), 1);
    o.see(1, false);
    CHECK_EQ(o.last(), -1);
}

TEST(window_order_a_window_opened_again_comes_last) {
    WindowOrder o;
    o.see(0, true);
    o.see(2, true);
    o.see(0, false);
    o.see(0, true);
    CHECK_EQ(o.last(), 0);
    o.see(2, true);  // open all along: its place stays
    CHECK_EQ(o.last(), 0);
    o.see(0, false);
    CHECK_EQ(o.last(), 2);
}

// 3.0.11-beta.106 (R-WCXUQP: «геймпад вкладки переключает не там который сейчас активен, например при выборе скилов
// переключает там где вещи открыты»): the driver's windows the controller drives - the last opened, or the one clicked
// in since (raise); a window not open is not raised
TEST(window_order_a_click_raises_an_open_window) {
    WindowOrder o;
    o.see(0, true);  // «Речі»
    o.see(2, true);  // «Навички» opened after it: the controller's
    CHECK_EQ(o.last(), 2);
    o.raise(0);  // a click in «Речі»
    CHECK_EQ(o.last(), 0);
    o.raise(4);  // not open: nothing
    CHECK_EQ(o.last(), 0);
    o.see(0, false);
    CHECK_EQ(o.last(), 2);
}

TEST(window_order_hint_names_b) {
    const std::vector<HintPart> p = windowHintParts(Lang::Uk);
    CHECK(p.size() == 1 && p[0].buttons == std::vector<uint16_t>({kPadB}) && p[0].words == "закрити вікно");
    CHECK(windowHint(Lang::En) == "B: close the window");
    CHECK(windowHint(Lang::Ru) == "B: закрыть окно");
}
