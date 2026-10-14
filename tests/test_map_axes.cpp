#include <cmath>
#include <vector>

#include "core/map_axes.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// The map file's torches of an uneven layout, seen where the game puts them with the axes `x` and `z` (a little off,
// as the flames are measured from the screen).
std::vector<map::Point> seenWith(const std::vector<map::Point>& placed, int x, int z) {
    std::vector<map::Point> seen;
    for (size_t i = 0; i < placed.size(); ++i) {
        const float d = float(i % 3) - 1.0f;
        seen.push_back(map::Point{x * placed[i].x + d, placed[i].y - 10.0f, z * placed[i].z - d});
    }
    return seen;
}

const std::vector<map::Point> kPlaced = {{-154.9f, 3.6f, 561.2f}, {-28.9f, 3.6f, 831.3f}, {-10.9f, 3.6f, 832.6f},
                                          {8.6f, 4.6f, 742.8f},    {-51.5f, 4.6f, 743.0f}, {222.5f, 9.6f, 700.9f},
                                          {301.0f, 2.0f, -95.0f},  {-410.0f, 6.0f, -320.0f}};

}  // namespace

TEST(map_axes_found_from_the_torches) {
    // 0.25: the map file's places and the game's world differ by the signs of x and z at most: the signs that put the
    // file's torches where the torches are seen.
    for (int x : {1, -1})
        for (int z : {1, -1}) {
            const map::Axes a = map::fitAxes(kPlaced, seenWith(kPlaced, x, z));
            CHECK(a.known());
            CHECK_EQ(int(a.x), x);
            CHECK_EQ(int(a.z), z);
        }
    // A few of them seen (the rest off the screen) are enough.
    const std::vector<map::Point> seen = seenWith(kPlaced, -1, 1);
    const map::Axes a = map::fitAxes(kPlaced, std::vector<map::Point>(seen.begin(), seen.begin() + 4));
    CHECK(a.known() && a.x == -1 && a.z == 1);
}

TEST(map_axes_unknown_when_it_cannot_tell) {
    CHECK(!map::fitAxes(kPlaced, {}).known());  // nothing seen
    // A symmetric layout fits two ways: no answer.
    const std::vector<map::Point> sym = {{-100, 0, 50}, {100, 0, 50}, {-100, 0, -50}, {100, 0, -50}};
    CHECK(!map::fitAxes(sym, seenWith(sym, 1, 1)).known());
    // Torches seen nowhere near the file's: no answer.
    std::vector<map::Point> far = seenWith(kPlaced, 1, 1);
    for (map::Point& p : far) p.x += 500.0f;
    CHECK(!map::fitAxes(kPlaced, far).known());
    // Into the world and back.
    const map::Axes a{-1, 1, 1};
    const map::Point w = map::toWorld(a, map::Point{10, -5, 20});
    CHECK(w.x == -10.0f && w.y == -5.0f && w.z == 20.0f);
}
