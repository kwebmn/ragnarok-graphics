#include <vector>

#include "core/item_pick.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.155 (the owner: several things of the storage picked with Ctrl and dragged at once, not one
// by one): the things picked by their places - Ctrl+click one on or off, Shift+click all between the last and
// this one (in the order shown), Ctrl+A all shown; those gone from the window dropped
TEST(item_pick_ctrl_shift_all_and_gone) {
    itempick::Picked p;
    const std::vector<int> shown = {10, 11, 12, 13, 14};
    p.toggle(11);
    p.toggle(13);
    CHECK(p.has(11) && p.has(13) && !p.has(12));
    p.toggle(13);
    CHECK(!p.has(13));
    p.range(shown, 14);  // from the last one clicked (13, let go) to 14
    CHECK(p.has(11) && p.has(13) && p.has(14) && !p.has(12) && !p.has(10));
    CHECK_EQ(p.count(), size_t(3));
    p.toggle(10);
    p.range(shown, 12);  // 10 .. 12
    CHECK(p.has(10) && p.has(11) && p.has(12));
    p.keepOnly({10, 11, 12});  // 13 and 14 taken
    CHECK_EQ(p.count(), size_t(3));
    p.all(shown);
    CHECK_EQ(p.count(), size_t(5));
    p.clear();
    CHECK_EQ(p.count(), size_t(0));
}
