// 3.0.11-beta.285 (the owner's feature 13): the gear of the bag against the one worn
#include "core/gear_compare.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(gear_compare_by_the_server_numbers) {
    const gearcompare::Gear* knife = gearcompare::find(1201);  // Knife: ATK 17, 3 slots, weapon level 1
    CHECK(knife != nullptr);
    CHECK_EQ(gearcompare::atkOf(*knife, 0), 17);
    CHECK_EQ(gearcompare::atkOf(*knife, 4), 25);
    CHECK_EQ(gearcompare::atkOf(*knife, 12), 17 + 20 + 2);  // past +10 half as much
    const gearcompare::Gear* shirt = gearcompare::find(2301);  // Cotton Shirt: DEF 1
    CHECK(shirt != nullptr);
    CHECK_EQ(gearcompare::defOf100(*shirt, 3), 310);
    CHECK(gearcompare::find(909) == nullptr);  // Jellopy: not gear
    const gearcompare::Texts t{"instead of {1}: {2}", "nothing worn: {1}", "slots"};
    CHECK_EQ(gearcompare::line(1201, 4, {}, t), std::string("nothing worn: ATK 25"));
    const std::vector<gearcompare::Worn> worn{{2301, 0, "Cotton Shirt"}, {1201, 0, "Knife"}};
    CHECK_EQ(gearcompare::line(1201, 4, worn, t), std::string("instead of Knife: ATK 17 \xE2\x86\x92 25 (+8)"));
    CHECK_EQ(gearcompare::line(909, 0, worn, t), std::string(""));
}
