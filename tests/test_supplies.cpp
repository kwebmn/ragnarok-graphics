// 3.0.16-beta.8 (the owner: «для магов и пристов показывать количество гемов, а для монков ... сферы»): what the bar's
// skills spend, by uAthena's skill_require_db
#include "core/supplies.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(supplies_by_the_skills_on_the_bar) {
    CHECK(supplies::itemsOf(27) == std::vector<int>{717});  // Warp Portal: a Blue Gemstone
    CHECK(supplies::itemsOf(362).size() == 4);               // Basilica: three gems and Holy Water
    CHECK(supplies::itemsOf(5).empty());                     // Bash spends nothing
    CHECK(supplies::itemsOf(116).empty());                   // a trap: its own count on its cell
    // a Priest's bar: Heal, Warp, Aspersio, Magnus - blue, holy water; no spheres
    const supplies::Pick priest = supplies::pick({28, 27, 68, 79}, 0);
    CHECK(priest.items == (std::vector<int>{717, 523}));
    CHECK(!priest.spheres);
    // at most three items, the bar's order
    const supplies::Pick hp = supplies::pick({362, 27}, 0);
    CHECK(hp.items == (std::vector<int>{715, 716, 717}));
    // a Monk: Summon Spirit Sphere - five dots; a Gunslinger's Flip the Coin - ten
    const supplies::Pick monk = supplies::pick({261, 267}, 0);
    CHECK(monk.spheres && monk.sphereMost == 5 && monk.items.empty());
    CHECK(supplies::pick({500}, 0).sphereMost == 10);
    CHECK(supplies::pick({}, 3).spheres);  // up now, nothing on the bar
    CHECK(!supplies::pick({}, 0).spheres);
}

TEST(supplies_level_by_count) {
    CHECK_EQ(supplies::level(25), 0);
    CHECK_EQ(supplies::level(10), 1);
    CHECK_EQ(supplies::level(1), 1);
    CHECK_EQ(supplies::level(0), 2);
}
