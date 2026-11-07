#include "core/world_signs.hpp"

#include "test.hpp"

using namespace uagfx;

// 3.0.16-beta.7: the signs over the characters - a room's kind, its places, a long title cut
TEST(world_signs_kinds_and_places) {
    CHECK(worldsigns::roomKind(0) == worldsigns::Kind::Locked);
    CHECK(worldsigns::roomKind(1) == worldsigns::Kind::Room);
    CHECK(worldsigns::accent(worldsigns::Kind::Shop) != worldsigns::accent(worldsigns::Kind::Room));
    CHECK(worldsigns::places(3, 20) == "3/20");
    CHECK(worldsigns::places(3, 0).empty());
}

TEST(world_signs_short_title) {
    CHECK(worldsigns::shortTitle("  Cheap pots!  ", 40) == "Cheap pots!");
    CHECK(worldsigns::shortTitle("Selling cards and more", 7) == "Selling\xE2\x80\xA6");
    // UTF-8: «Продаю» is 6 letters, 12 bytes
    const std::string sells = "\xD0\x9F\xD1\x80\xD0\xBE\xD0\xB4\xD0\xB0\xD1\x8E";
    CHECK(worldsigns::shortTitle(sells + " \xD0\xB7\xD1\x96\xD0\xBB\xD0\xBB\xD1\x8F", 6) == sells + "\xE2\x80\xA6");
}
