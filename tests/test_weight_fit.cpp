#include "core/weight_fit.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.156 (the owner: how many can be carried counted ahead, potions by weight): how many of a thing the
// bag takes - the room to the weight's most over the thing's weight (the game keeps whole units, a tenth of the
// server's: one unit kept spare)
TEST(weight_fit_how_many_go_in) {
    CHECK_EQ(weightfit::fits(1000, 2000, 70), 142);  // 999 units free x10 / 7.0 each
    CHECK_EQ(weightfit::fits(1990, 2000, 70), 1);   // 9 units free: one of 7.0
    CHECK_EQ(weightfit::fits(2000, 2000, 70), 0);
    CHECK_EQ(weightfit::fits(100, 2000, 0), -1);     // weightless: no limit
    CHECK_EQ(weightfit::fits(0, 0, 70), -1);         // the most not known
    CHECK_EQ(weightfit::fits(1000, 2000, 70, 70), 57);  // to 70 % (the regen's line on uaRO): 399 x10 / 70
}
