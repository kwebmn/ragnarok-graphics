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

// 3.0.11-beta.111 (question 1: «два окна вплотную друг к другу как одно окно»): «Спорядження» and «Речі» open together
// stand side by side, tops in line - the gear left of the bag where there is room, else right of it; the one led keeps
// to the leader's side
TEST(window_stack_docks_the_equipment_beside_the_bag) {
    using winstack::Box;
    using winstack::Side;
    const Box bag{800, 200, 600, 700}, gear{300, 150, 450, 600};
    CHECK(winstack::dockSide(bag, gear, 1920) == Side::Left);
    const Box left = winstack::docked(bag, gear, Side::Left);
    CHECK(left.x == 350 && left.y == 200 && left.w == 450 && left.h == 600);
    const Box narrow{100, 200, 600, 700};
    CHECK(winstack::dockSide(narrow, gear, 1920) == Side::Right);  // no room on its left
    CHECK(winstack::docked(narrow, gear, Side::Right).x == 700);
    CHECK(winstack::docked(gear, bag, Side::Right).x == 750);  // the gear dragged: the bag follows on its right
    CHECK(winstack::opposite(Side::Left) == Side::Right);
}


// 3.0.22-beta.40 (the owner's interface over the MCP: the character block's size grip over «Навички»): a point under a
// window of the stack - the panels under it show no grip there
TEST(window_stack_covers_a_point) {
    const std::vector<winstack::Box> open = {{329, 191, 1385, 708}, {10, 10, 100, 50}};
    CHECK(winstack::covers(open, 492, 268));
    CHECK(winstack::covers(open, 20, 20));
    CHECK(!winstack::covers(open, 300, 100));  // the character block alone
    CHECK(!winstack::covers(open, 1714, 899));  // just past the corner
    CHECK(!winstack::covers({}, 492, 268));
}

// 3.0.24-beta.56 (the owner's «давай», item 6: the keyboard concept's window switch - Ctrl+Tab the chat's channels):
// Alt+` the open windows' round - the bottom one raised each press (the keys' frame goes to the top), all of them in
// turn; one window or none: nothing to switch
TEST(window_stack_alt_grave_rounds_the_open_windows) {
    using winstack::Win;
    CHECK(winstack::roundNext({Win::Items, Win::Stats, Win::Skills}) == Win::Skills);
    CHECK(!winstack::roundNext({Win::Items}).has_value());
    CHECK(!winstack::roundNext({}).has_value());
    winstack::Stack st;
    st.see(Win::Skills, true);
    st.see(Win::Stats, true);
    st.see(Win::Items, true);  // opened last: the top
    std::vector<Win> seen;
    for (int i = 0; i < 3; ++i) {
        std::vector<Win> open;
        for (const Win w : st.topDown())
            if (w == Win::Items || w == Win::Stats || w == Win::Skills) open.push_back(w);
        const std::optional<Win> next = winstack::roundNext(open);
        CHECK(next.has_value());
        if (!next) return;
        st.raise(*next);
        seen.push_back(*next);
    }
    CHECK(seen == (std::vector<Win>{Win::Skills, Win::Stats, Win::Items}));  // each in turn, back to the first
}
