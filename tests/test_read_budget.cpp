// 3.0.20-beta.2 (R-JJE8HA: the tester's «Склад» on the release - its icons read one after another on the game's thread
// from a slow disk, the game still for 15 s): the first reads kept to a budget a frame's time, the rest in the next
#include "core/read_budget.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(read_budget_spends_its_share_of_a_window_then_waits_for_the_next) {
    readbudget::Budget b(4.0, 16.0);
    CHECK(b.allowed(100.0));
    b.spend(2.5);
    CHECK(b.allowed(101.0));
    b.spend(2.0);                 // 4.5 of 4 in this window
    CHECK(!b.allowed(105.0));
    CHECK(!b.allowed(115.9));
    CHECK(b.allowed(116.1));      // the next window
    b.spend(30.0);                // one slow read: the window's whole share and more
    CHECK(!b.allowed(120.0));
    CHECK(b.allowed(132.2));      // never more than the window it was spent in
}
