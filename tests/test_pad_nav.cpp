#include <vector>

#include "core/pad_nav.hpp"
#include "test.hpp"

using namespace uagfx;
using padnav::Dir;

// 3.0.18-beta.60: the D-pad walks a window's boxes - a grid by rows and columns, a button row under it
TEST(pad_nav_walks_a_grid_and_the_buttons_under_it) {
    std::vector<panel::Box> t;
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 3; ++c) t.push_back(panel::Box{10.0f + 50 * c, 40.0f + 50 * r, 40, 40});  // 0..5
    t.push_back(panel::Box{20, 150, 60, 24});   // 6 «Зберегти»
    t.push_back(panel::Box{100, 150, 60, 24});  // 7 «Скасувати»
    t.push_back(panel::Box{170, 6, 20, 20});    // 8 ×
    CHECK_EQ(padnav::first(t), 8);  // the top: the × above the grid
    CHECK_EQ(padnav::next(t, 0, Dir::Right), 1);
    CHECK_EQ(padnav::next(t, 1, Dir::Down), 4);
    CHECK_EQ(padnav::next(t, 4, Dir::Left), 3);
    CHECK_EQ(padnav::next(t, 3, Dir::Left), 3);  // nothing to its left: stays
    CHECK_EQ(padnav::next(t, 3, Dir::Down), 6);
    CHECK_EQ(padnav::next(t, 5, Dir::Down), 7);
    CHECK_EQ(padnav::next(t, 6, Dir::Right), 7);
    CHECK_EQ(padnav::next(t, 2, Dir::Up), 8);
    CHECK_EQ(padnav::next(t, -1, Dir::Down), 8);  // none yet: the first
    CHECK_EQ(padnav::next({}, -1, Dir::Down), -1);
}
