// 3.0.11-beta.68 (the concept's «Ваша панель: Fire Bolt ×2…» - the owner: «отличная идея, нужна»): the player's
// hotbar skills against a monster's element - the server's elements of the skills and its multipliers
#include <string>
#include <vector>

#include "core/bar_hint.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

TEST(bar_hint_skill_elements_by_the_server) {
    CHECK_EQ(barhint::elementOf("MG_FIREBOLT"), 3);
    CHECK_EQ(barhint::elementOf("WZ_STORMGUST"), 1);
    CHECK_EQ(barhint::elementOf("MG_SOULSTRIKE"), 8);
    CHECK_EQ(barhint::elementOf("AL_BLESSING"), -1);  // no damage
    CHECK_EQ(barhint::elementOf("AC_DOUBLE"), -1);    // the weapon's element
    CHECK_EQ(barhint::elementOf("NOT_A_SKILL"), -1);
}

TEST(bar_hint_against_a_wraith) {
    const std::vector<barhint::Skill> bar{{"WZ_JUPITEL", "Jupitel Thunder"}, {"MG_FIREBOLT", "Fire Bolt"},
                                          {"AL_BLESSING", "Blessing"},       {"WZ_STORMGUST", "Storm Gust"},
                                          {"MG_FIREBOLT", "Fire Bolt"},      {"WZ_EARTHSPIKE", "Earth Spike"}};
    const std::vector<barhint::Hit> h = barhint::against(bar, 9, 4);  // Wraith: undead 4
    CHECK_EQ(h.size(), size_t(4));                                     // each once, Blessing none
    if (h.size() != 4) return;
    CHECK(h[0].name == "Fire Bolt" && h[0].percent == 200);
    CHECK(h[1].name == "Storm Gust" && h[1].percent == 150);
    CHECK(h[2].name == "Jupitel Thunder" && h[2].percent == 100);
    CHECK(h[3].name == "Earth Spike" && h[3].percent == 50);
    CHECK_EQ(barhint::line(h, Lang::Uk),
             std::string("Fire Bolt \xC3\x97" "2 \xC2\xB7 Storm Gust \xC3\x97" "1,5 \xC2\xB7 Jupitel Thunder \xC3\x97"
                         "1 \xC2\xB7 Earth Spike \xC3\x97" "0,5"));
    CHECK_EQ(barhint::line(h, Lang::En, 2), std::string("Fire Bolt \xC3\x97" "2 \xC2\xB7 Storm Gust \xC3\x97" "1.5"));
    // holy on holy 3 heals it
    const std::vector<barhint::Hit> holy = barhint::against({{"AL_HOLYLIGHT", "Holy Light"}}, 6, 3);
    CHECK(holy.size() == 1 && holy[0].percent == -50);
    CHECK_EQ(barhint::line(holy, Lang::Uk), std::string("Holy Light \xD0\xBB\xD1\x96\xD0\xBA\xD1\x83\xD1\x94"));
    CHECK(barhint::against({{"AL_BLESSING", "Blessing"}}, 9, 4).empty());
    CHECK(barhint::line({}, Lang::Uk).empty());
}
