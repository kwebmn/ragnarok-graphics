#include <algorithm>
#include <cmath>

#include "core/trample.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

footsteps::Step stepAt(float x, float z, float born) {
    footsteps::Step s;
    s.x = x;
    s.z = z;
    s.born = born;
    return s;
}

bool near(float a, float b, float within) { return std::fabs(a - b) <= within; }

}  // namespace

TEST(trample_grass_roots_on_every_second_row_at_twice_the_size) {
    // 2.7.29 (the user: «есть мысль, что шар подтормаживает графику, можешь оптимизировать?» - the grass 3-5 ms of
    // 8-13 at 4K, R-PDY4DY): at twice the game's size and more a game pixel is two rows and more - grass_ps looks for
    // the blades' roots on every second row (as Direct3D 11 always), the same blades for half the work
    CHECK_EQ(trample::rootRows(1.0f), 1);
    CHECK_EQ(trample::rootRows(1.25f), 1);
    CHECK_EQ(trample::rootRows(1.5f), 2);
    CHECK_EQ(trample::rootRows(2.0f), 2);
    CHECK_EQ(trample::rootRows(3.0f), 2);  // grass_ps's rows: one or every second
}

