#include "core/card_names.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.112 (the owner: «там вещи подписывались типа Saharic Jur»): a thing with cards named as the game does
TEST(card_names_as_the_game_names_them) {
    const cardnames::Tables t =
        cardnames::read("4001#Lucky#\r\n4035#Bloody#\r\n4014#Counter#\r\n// a remark\r\n4064#Saharic#\r\n",
                        "4014#\r\n");
    const int one[4] = {4064, 0, 0, 0};
    CHECK_EQ(cardnames::named("Jur", one, t), std::string("Saharic Jur"));
    const int two[4] = {4035, 4035, 0, 0};
    CHECK_EQ(cardnames::named("Katana", two, t), std::string("Double Bloody Katana"));
    const int mixed[4] = {4035, 4001, 4035, 4035};
    CHECK_EQ(cardnames::named("Katana", mixed, t), std::string("Triple Bloody Lucky Katana"));
    const int after[4] = {4014, 0, 0, 0};
    CHECK_EQ(cardnames::named("Clip", after, t), std::string("Clip of Counter"));
    const int forged[4] = {0xFF, 4035, 0, 0};  // a forged thing's maker: its own name
    CHECK_EQ(cardnames::named("Katana", forged, t), std::string("Katana"));
    const int unknown[4] = {9999, 0, 0, 0};
    CHECK_EQ(cardnames::named("Katana", unknown, t), std::string("Katana"));
}
