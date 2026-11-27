// 3.0.24-beta.22 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Скільки штук»: «предел по весу
// и месту: «у склад влізе 87»»; the plan's task 7): how many of a stack fit where it goes, and the count's quick
// choices 1 / 10 / half / all
#include "core/ask_card.hpp"
#include "core/count_limit.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(count_limit_fits_by_weight_and_place) {
    // 30 000 of a thing weighing 1 (10 tenths) with 87 of weight free: 87
    CHECK_EQ(countlimit::most(30000, 10, 870, -1, false), 87);
    CHECK_EQ(countlimit::most(30000, 10, -1, -1, false), 30000);  // no weight's limit (the storage)
    CHECK_EQ(countlimit::most(50, -1, 870, -1, false), 50);       // its weight not known: as it is
    CHECK_EQ(countlimit::most(50, 10, 870, 0, false), 0);         // no free place, not there to stack on
    CHECK_EQ(countlimit::most(50, 10, 870, 0, true), 50);         // no free place, but stacks on its own kind
    CHECK_EQ(countlimit::most(50, 10, -5, -1, false), 0);         // over the weight already
}

TEST(count_limit_quick_choices_one_ten_half_all) {
    CHECK_EQ(askcard::quickValue(askcard::Quick::One, 30), 1);
    CHECK_EQ(askcard::quickValue(askcard::Quick::Ten, 30), 10);
    CHECK_EQ(askcard::quickValue(askcard::Quick::Ten, 7), 7);
    CHECK_EQ(askcard::quickValue(askcard::Quick::Half, 7), 3);
    CHECK_EQ(askcard::quickValue(askcard::Quick::All, 7), 7);
    CHECK_EQ(askcard::kQuicks, 4);
}
