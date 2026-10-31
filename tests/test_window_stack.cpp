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

// 3.0.11-beta.109 (question 4 «ок»): a window opening where another one already stands goes beside it - right of it,
// else left, while the screen holds it - else 24 pixels down and right of it; a free place, or a sliver shared, stays
TEST(window_stack_a_new_window_beside_the_one_in_its_place) {
    using winstack::Box;
    const std::vector<Box> open = {Box{100, 100, 400, 300}};
    const auto same = [](Box a, Box b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; };
    CHECK(same(winstack::place(Box{600, 100, 300, 200}, open, 1920, 1080, 8, 24), Box{600, 100, 300, 200}));  // free
    CHECK(same(winstack::place(Box{480, 120, 300, 200}, open, 1920, 1080, 8, 24), Box{480, 120, 300, 200}));  // sliver
    CHECK(same(winstack::place(Box{150, 150, 300, 200}, open, 1920, 1080, 8, 24), Box{508, 100, 300, 200}));  // right
    CHECK(same(winstack::place(Box{150, 150, 300, 200}, open, 700, 1080, 8, 24), Box{124, 124, 300, 200}));
    const Box left = winstack::place(Box{1300, 150, 300, 200}, {Box{1200, 100, 700, 300}}, 1920, 1080, 8, 24);
    CHECK(same(left, Box{892, 100, 300, 200}));  // no room right: left
    const Box cascade = winstack::place(Box{150, 150, 600, 200}, {Box{100, 100, 1500, 300}}, 1920, 1080, 8, 24);
    CHECK(same(cascade, Box{124, 124, 600, 200}));  // no room either side: down and right
    const Box held = winstack::place(Box{1700, 1000, 300, 200}, {}, 1920, 1080, 8, 24);
    CHECK(same(held, Box{1620, 880, 300, 200}));  // inside the screen
}

