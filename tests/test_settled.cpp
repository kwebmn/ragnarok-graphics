#include "core/settled.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(settled_takes_the_first_value_at_once) {
    Settled s(300);
    CHECK(s.take(1.75, 1000));
    CHECK(s.taken() == 1.75);
    CHECK(!s.take(1.75, 1001));  // the same: nothing to take
}

TEST(settled_takes_a_change_once_it_holds) {
    Settled s(300);
    s.take(1.75, 0);
    CHECK(!s.take(1.8, 100));  // the window's edge is being dragged
    CHECK(!s.take(1.9, 150));
    CHECK(!s.take(1.9, 400));  // 250 ms of 1.9
    CHECK(s.take(1.9, 450));   // 300 ms: taken
    CHECK(s.taken() == 1.9);
    CHECK(!s.take(1.9, 900));
}

TEST(settled_a_change_back_is_no_change) {
    Settled s(300);
    s.take(1.75, 0);
    CHECK(!s.take(2.0, 10));
    CHECK(!s.take(1.75, 20));  // back where it was
    CHECK(!s.take(2.0, 400));  // a new wait from here
    CHECK(s.take(2.0, 700));
}
