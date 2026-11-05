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
