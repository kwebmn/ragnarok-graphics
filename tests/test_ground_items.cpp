// 3.0.11-beta.283 (the owner's features 11 and 12): the items on the ground from the server's packets
#include "core/ground_items.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ground_items_seen_dropped_and_gone) {
    grounditems::Ground g;
    // 0x009d: id 0x01020304, item 909 (Jellopy), identified, 150, 200, amount 3
    const uint8_t seen[17] = {0x9d, 0, 4, 3, 2, 1, 0x8d, 0x03, 1, 150, 0, 200, 0, 3, 0, 5, 7};
    g.packet(0x009d, seen);
    // 0x009e: id 7, item 4001 (Poring Card), 10, 11, sub 2 3, amount 1
    const uint8_t fell[17] = {0x9e, 0, 7, 0, 0, 0, 0xa1, 0x0f, 1, 10, 0, 11, 0, 2, 3, 1, 0};
    g.packet(0x009e, fell);
    CHECK_EQ(g.items().size(), size_t(2));
    const grounditems::Item& a = g.items().at(0x01020304u);
    CHECK_EQ(a.item, 909);
    CHECK_EQ(a.x, 150);
    CHECK_EQ(a.y, 200);
    CHECK_EQ(a.amount, 3);
    CHECK_EQ(g.items().at(7u).amount, 1);
    CHECK_EQ(g.items().at(7u).item, 4001);
    const uint8_t gone[6] = {0xa1, 0, 4, 3, 2, 1};
    g.packet(0x00a1, gone);
    CHECK_EQ(g.items().size(), size_t(1));
}

TEST(ground_items_worth_and_label) {
    CHECK(grounditems::worth(6, 20) == grounditems::Worth::Card);
    CHECK(grounditems::worth(4, 0) == grounditems::Worth::Gear);
    CHECK(grounditems::worth(3, 10000) == grounditems::Worth::Dear);
    CHECK(grounditems::worth(3, 9999) == grounditems::Worth::Plain);
    CHECK_EQ(grounditems::label("Jellopy", 3), std::string("Jellopy \xC3\x97" "3"));
    CHECK_EQ(grounditems::label("Poring Card", 1), std::string("Poring Card"));
}

// 3.0.20-beta.44 (R-RDA885 «викидав предмети 30 хвилин назад на землі їх вже нема але описання осталось»): the server
// tells an item gone (0x00a1) only to those who see it then - after a warp within the same map, a respawn or a new
// login on it nobody tells, and the map's name is the same: the names stayed for good. Entering a map (0x0073 after
// the login, 0x0091 a warp, 0x0092 another map server's) forgets them all - the server tells those in sight again;
// and one not told gone for kMostLife is forgotten by itself (the server keeps a dropped item a minute)
TEST(ground_items_forgotten_at_a_maps_entry_and_when_too_old) {
    const uint8_t fell[17] = {0x9e, 0, 7, 0, 0, 0, 0xa1, 0x0f, 1, 10, 0, 11, 0, 2, 3, 1, 0};
    const uint8_t seen[17] = {0x9d, 0, 4, 3, 2, 1, 0x8d, 0x03, 1, 150, 0, 200, 0, 3, 0, 5, 7};
    for (const uint16_t entry : {uint16_t(0x0073), uint16_t(0x0091), uint16_t(0x0092)}) {
        grounditems::Ground g;
        g.packet(0x009e, fell, 1000);
        g.packet(0x009d, seen, 1000);
        const uint8_t enter[28] = {uint8_t(entry & 0xFF), uint8_t(entry >> 8)};
        g.packet(entry, enter, 2000);
        CHECK(g.items().empty());
    }
    grounditems::Ground g;
    g.packet(0x009e, fell, 1000);
    g.packet(0x009d, seen, 1000 + grounditems::kMostLife - 5000);  // seen later: its own time
    g.forgetOld(1000 + grounditems::kMostLife - 1);
    CHECK_EQ(g.items().size(), size_t(2));  // not yet
    g.forgetOld(1000 + grounditems::kMostLife + 1);
    CHECK(g.items().size() == 1 && g.items().count(0x01020304u) == 1);  // the first one's time is up
    g.packet(0x009d, seen, 1000 + grounditems::kMostLife + 100);  // told again: it lies there, from now
    g.forgetOld(1000 + 2 * grounditems::kMostLife);
    CHECK_EQ(g.items().size(), size_t(1));
    // and one the player has walked well out of sight of (uAthena shows 40 cells round, area_size; told again when
    // back in sight)
    g.packet(0x009e, fell, 5000);  // at 10, 11; the other at 150, 200
    g.forgetFar(150, 190, grounditems::kOutOfSight);
    CHECK(g.items().size() == 1 && g.items().count(0x01020304u) == 1);
    g.forgetFar(150, 200 + grounditems::kOutOfSight, grounditems::kOutOfSight);  // at the range's edge: kept
    CHECK_EQ(g.items().size(), size_t(1));
    g.forgetFar(150 + grounditems::kOutOfSight + 1, 200, grounditems::kOutOfSight);
    CHECK(g.items().empty());
}
