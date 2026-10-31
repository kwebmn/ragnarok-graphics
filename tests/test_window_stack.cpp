// 3.0.11-beta.108 (the concept «Концепция окон и инвентаря», stage 1; the owner 31.10: «окна одно поверх другого не
// появляются», answers 4-8 «ок»): the driver's windows in one stack - drawn bottom up, the mouse, the keys and the
// controller the top's; a window opened or clicked in comes on top; the top one of two or more gets the gold edge
#include <vector>

#include "core/window_stack.hpp"
#include "test.hpp"

using namespace uagfx;
using winstack::Win;

TEST(window_stack_the_last_opened_or_clicked_on_top) {
    winstack::Stack s;
    CHECK(!s.top());
    s.see(Win::Items, true);
    s.see(Win::Skills, true);  // opened after: on top
    CHECK(s.top() && *s.top() == Win::Skills);
    CHECK(s.bottomUp() == std::vector<Win>({Win::Items, Win::Skills}));
    s.see(Win::Items, true);  // open all along: its place kept
    CHECK(*s.top() == Win::Skills);
    s.raise(Win::Items);  // a click in it
    CHECK(*s.top() == Win::Items && s.bottomUp().front() == Win::Skills);
    s.raise(Win::Shop);  // not open: nothing
    CHECK(s.bottomUp().size() == 2);
    s.see(Win::Items, false);  // closed
    CHECK(*s.top() == Win::Skills);
    CHECK(!s.marked(Win::Skills));  // alone: no gold edge
    s.see(Win::Equip, true);
    CHECK(s.marked(Win::Equip) && !s.marked(Win::Skills));
    CHECK(s.topDown() == std::vector<Win>({Win::Equip, Win::Skills}));
}
