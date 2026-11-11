#include <cstdint>
#include "core/card_names.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.112 (the owner: «там вещи подписывались типа Saharic Jur»): a thing with cards named as the game does
TEST(card_names_as_the_game_names_them) {
    const cardnames::Tables t =
        cardnames::read("4001#Lucky#\r\n4035#Bloody#\r\n4014#of Champion#\r\n// a remark\r\n4064#Saharic#\r\n",
                        "4014#\r\n");
    const int one[4] = {4064, 0, 0, 0};
    CHECK_EQ(cardnames::named("Jur", one, t), std::string("Saharic Jur"));
    const int two[4] = {4035, 4035, 0, 0};
    CHECK_EQ(cardnames::named("Katana", two, t), std::string("Double Bloody Katana"));
    const int mixed[4] = {4035, 4001, 4035, 4035};
    CHECK_EQ(cardnames::named("Katana", mixed, t), std::string("Triple Bloody Lucky Katana"));
    const int after[4] = {4014, 0, 0, 0};
    // 3.0.18-beta.113 (R-55WR4R «Clip of of Counter»): the table's word after the name has its «of» already
    CHECK_EQ(cardnames::named("Clip", after, t), std::string("Clip of Champion"));
    const int twice[4] = {4014, 4014, 0, 0};
    CHECK_EQ(cardnames::named("Clip", twice, t), std::string("Clip of Double Champion"));
    const int forged[4] = {0xFF, 0, 4035, 0};  // a forged thing, no crumbs, no element: its own name (.142)
    CHECK_EQ(cardnames::named("Katana", forged, t), std::string("Katana"));
    const int unknown[4] = {9999, 0, 0, 0};
    CHECK_EQ(cardnames::named("Katana", unknown, t), std::string("Katana"));
}

// 3.0.18-beta.118 (the owner: «говорят что в вендинге не видно карт»): a thing's cards from its ITEM_INFO (+0x1C, four
// ints - the server's 0x0133 put there by FUN_0058aa10, the bag's and the storage's alike), its name with them only
// when identified (+0x3C)
TEST(card_names_from_item_info) {
    uint8_t info[0x48] = {};
    const auto put = [&](size_t at, int v) {
        for (int b = 0; b < 4; ++b) info[at + size_t(b)] = uint8_t(uint32_t(v) >> (8 * b));
    };
    put(0x1C, 4001);
    put(0x20, 4001);
    put(0x24, 4014);
    cardnames::Tables t = cardnames::read("4001#Lucky#\n4014#of Counter#\n", "4014#\n");
    int cards[4] = {9, 9, 9, 9};
    cardnames::cardsOf(info, cards);
    CHECK(cards[0] == 4001 && cards[1] == 4001 && cards[2] == 4014 && cards[3] == 0);
    CHECK_EQ(cardnames::namedFromInfo("Katana", info, t), std::string("Katana"));  // not identified: as it is
    info[0x3C] = 1;
    CHECK_EQ(cardnames::namedFromInfo("Katana", info, t), std::string("Double Lucky Katana of Counter"));
}

// 3.0.18-beta.142 (the owner asked about things forged by a smith): a forged thing (255 in its first place) named
// as the game's own words have it - its star crumbs (the second place's high byte 5 / 10 / 15: «Very Strong» ...)
// and its element (its low byte: 1 water «Ice», 2 earth, 3 fire, 4 wind); its maker's character in the last two
TEST(card_names_forged_things) {
    const cardnames::Tables t;
    const int fire2[4] = {255, (10 << 8) | 3, 150001, 0};
    CHECK_EQ(cardnames::named("Main Gauche", fire2, t), std::string("Very Very Strong Fire Main Gauche"));
    const int plain[4] = {255, 0, 7, 1};
    CHECK_EQ(cardnames::named("Knife", plain, t), std::string("Knife"));
    const int ice3[4] = {255, (15 << 8) | 1, 0, 0};
    CHECK_EQ(cardnames::named("Axe", ice3, t), std::string("Very Very Very Strong Ice Axe"));
    const int wind[4] = {255, 4, 0, 0};
    CHECK_EQ(cardnames::named("Sword", wind, t), std::string("Wind Sword"));
    CHECK(cardnames::forged(fire2) && cardnames::forged(plain));
    const int none[4] = {4001, 0, 0, 0};
    CHECK(!cardnames::forged(none));
    CHECK_EQ(cardnames::makerOf(plain), 1 * 0x10000 + 7);  // the third place low, the fourth high
}
