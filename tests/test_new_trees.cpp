#include "core/new_trees.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(new_trees_steady_keeps_a_crown_through_a_hair_of_movement) {
    new_trees::Steady st;
    float lo[3] = {10.0f, -20.0f, 30.0f}, hi[3] = {16.0f, -10.0f, 36.0f};
    const uint32_t seed = st.settle(7, lo, hi);
    st.endFrame();
    float lo2[3] = {10.24f, -20.1f, 29.8f}, hi2[3] = {16.26f, -9.9f, 35.75f};  // read back a little off
    CHECK(st.settle(7, lo2, hi2) == seed);
    CHECK(lo2[0] == 10.0f && hi2[2] == 36.0f);  // the kept box
    // a second crown of the tree, close by, keeps its own
    float lo3[3] = {10.1f, -20.0f, 30.1f}, hi3[3] = {16.1f, -10.0f, 36.1f};
    CHECK(st.settle(7, lo3, hi3) != seed);
    st.endFrame();
    for (int k = 0; k < 130; ++k) st.endFrame();
    CHECK(st.size() == 0);  // long unseen: gone
}
