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

// 3.0.16-beta.13: what one use needs - spheres by the skill (Finger Offensive by its level), Fire Pillar's gem
TEST(supplies_spheres_and_items_at_a_level) {
    CHECK_EQ(supplies::spheresFor(271, 5), 5);
    CHECK_EQ(supplies::spheresFor(267, 3), 3);
    CHECK_EQ(supplies::spheresFor(372, 1), 2);
    CHECK_EQ(supplies::spheresFor(28, 10), 0);
    CHECK(supplies::itemsAt(80, 5).empty());
    CHECK(supplies::itemsAt(80, 6) == std::vector<int>{717});
}
