// 3.0.11-beta.281 (the owner's feature 16): the hotbar's healing items running low
#include "core/potion_warn.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(potion_warn_falls_and_runs_out) {
    potionwarn::Watch w;
    CHECK_EQ(w.step({{501, 12}, {502, 3}}, true), 0);  // the first look tells nothing
    CHECK_EQ(w.step({{501, 11}, {502, 3}}, true), 0);
    CHECK_EQ(w.step({{501, 10}, {502, 3}}, true), 2);
    CHECK_EQ(w.step({{501, 9}, {502, 3}}, true), 0);
    CHECK_EQ(w.step({}, false), 0);                     // a map loading
    CHECK_EQ(w.step({{501, 9}, {502, 0}}, true), 3);
    CHECK_EQ(w.step({{501, 9}, {502, 0}}, true), 0);
    CHECK_EQ(w.step({{501, 30}, {502, 0}, {503, 1}}, true), 0);  // bought, a new one on the bar
    CHECK(potionwarn::low(10) && !potionwarn::low(11) && !potionwarn::low(0));
}
