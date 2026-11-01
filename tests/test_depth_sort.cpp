// 3.0.11-beta.148 (R-DEHAHA: uaRO.exe crashed at 0x416C29 two seconds into prt_in): the game's sort of its see-through
// faces by depth (core/depth_sort) - a depth not a number broke its order and its scan ran past the list's end
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "core/depth_sort.hpp"
#include "test.hpp"

using namespace uagfx;

// The game's comparison (0x416C20): a depth not a number sorts before every other one and every other one before it -
// no order at all; mended, it is the farthest (drawn first), the others as they were, an order again
TEST(depth_sort_a_depth_not_a_number_mended) {
    using depthsort::Pair;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    CHECK(depthsort::gameLess(Pair{nan, 1}, Pair{0.5f, 2}) && depthsort::gameLess(Pair{0.5f, 2}, Pair{nan, 1}));
    CHECK(depthsort::gameLess(Pair{0.5f, 1}, Pair{0.5f, 2}) && !depthsort::gameLess(Pair{0.5f, 2}, Pair{0.5f, 1}));
    std::vector<Pair> v = {{0.3f, 1}, {nan, 2}, {0.1f, 3}, {inf, 4}, {-nan, 5}, {0.2f, 6}};
    CHECK(depthsort::mend(v.data(), v.data() + v.size()) == 2);
    CHECK(v[0].depth == 0.3f && v[2].depth == 0.1f && v[3].depth == inf && v[5].depth == 0.2f);
    CHECK(v[1].depth == -std::numeric_limits<float>::max() && v[1].face == 2 && v[4].face == 5);
    for (const Pair& a : v)
        for (const Pair& b : v) CHECK(!(depthsort::gameLess(a, b) && depthsort::gameLess(b, a)));
    std::sort(v.begin(), v.end(), depthsort::gameLess);
    CHECK(v[0].face == 2 && v[1].face == 5 && v[2].face == 3 && v.back().face == 4);
    CHECK(depthsort::mend(v.data(), v.data()) == 0 && depthsort::mend(nullptr, nullptr) == 0);
}
