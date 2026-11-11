#include <string>

#include "core/item_elements.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.144 (the owner: element things such as a wind weapon in their own colour): a thing's element -
// the server's table (Fire Brand 1133 fire, Ice Falchion 1131 water; Pasana Card 4099 makes the armour fire), a
// forged one's own first; its colour and word
TEST(item_elements_by_table_forge_and_card) {
    CHECK_EQ(itemelements::ofItem(1133), 3);
    CHECK_EQ(itemelements::ofItem(1131), 1);
    CHECK_EQ(itemelements::ofItem(1201), 0);
    const int none[4] = {0, 0, 0, 0};
    CHECK_EQ(itemelements::ofThing(1133, none), 3);
    const int pasana[4] = {4099, 0, 0, 0};
    CHECK_EQ(itemelements::ofThing(2307, pasana), 3);  // a Mantle with Pasana: fire
    const int wind[4] = {255, (5 << 8) | 4, 0, 0};
    CHECK_EQ(itemelements::ofThing(1201, wind), 4);  // a forged wind knife
    CHECK_EQ(itemelements::ofThing(1133, wind), 4);  // forged's own first
    CHECK(itemelements::colour(4) != 0 && itemelements::colour(3) != itemelements::colour(4));
    CHECK_EQ(itemelements::colour(0), 0u);
    CHECK_EQ(std::string(itemelements::word(4)), std::string("Wind"));
    CHECK_EQ(std::string(itemelements::word(0)), std::string(""));
}
