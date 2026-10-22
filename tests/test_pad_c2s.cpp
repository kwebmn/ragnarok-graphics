#include "pad/core/proto/c2s.hpp"
#include "pad/core/proto/packet_tables.hpp"
#include "test.hpp"

using namespace uapad;

TEST(c2s_walk_layout) {
    Bytes b = buildWalk(Cell{150, 287});
    CHECK_EQ(b.size(), 8u);
    CHECK_EQ(rd16(b.data(), 0), 0x00a7);
    CHECK_EQ(b[5], 37);
    CHECK_EQ(b[6], 0x91);
    CHECK_EQ(b[7], 0xf0);
    auto c = parseWalkRequest(b);
    CHECK(c.has_value());
    CHECK(c && *c == (Cell{150, 287}));
}

TEST(c2s_action_layout) {
    Bytes b = buildAction(110000123, kActionAttack);
    CHECK_EQ(b.size(), 19u);
    CHECK_EQ(rd16(b.data(), 0), 0x0190);
    CHECK_EQ(rd32(b.data(), 5), 110000123u);
    CHECK_EQ(b[18], 7);
    Bytes sit = buildAction(0, kActionSit);
    CHECK_EQ(sit[18], 2);
}

TEST(c2s_skill_to_id_layout) {
    Bytes b = buildSkillToId(5, 10, 110000123);  // Bash lv 10
    CHECK_EQ(b.size(), 25u);
    CHECK_EQ(rd16(b.data(), 0), 0x0072);
    CHECK_EQ(rd16(b.data(), 6), 10);
    CHECK_EQ(rd16(b.data(), 10), 5);
    CHECK_EQ(rd32(b.data(), 21), 110000123u);
}

TEST(c2s_skill_to_pos_layout) {
    Bytes b = buildSkillToPos(12, 1, Cell{100, 200});  // Safety Wall
    CHECK_EQ(b.size(), 22u);
    CHECK_EQ(rd16(b.data(), 0), 0x0113);
    CHECK_EQ(rd16(b.data(), 5), 1);
    CHECK_EQ(rd16(b.data(), 9), 12);
    CHECK_EQ(rd16(b.data(), 12), 100);
    CHECK_EQ(rd16(b.data(), 20), 200);
}

TEST(c2s_take_and_use_item_layout) {
    Bytes t = buildTakeItem(4242);
    CHECK_EQ(t.size(), 8u);
    CHECK_EQ(rd16(t.data(), 0), 0x00f5);
    CHECK_EQ(rd32(t.data(), 4), 4242u);
    Bytes u = buildUseItem(7, 2000001);
    CHECK_EQ(u.size(), 14u);
    CHECK_EQ(rd16(u.data(), 0), 0x009f);
    CHECK_EQ(rd16(u.data(), 4), 7);
    CHECK_EQ(rd32(u.data(), 10), 2000001u);
}

TEST(c2s_parse_want_to_connection) {
    Bytes b(26, 0);
    wr16(b, 0, 0x009b);
    wr32(b, 4, 2000001);
    auto id = parseWantToConnection(b);
    CHECK(id && *id == 2000001u);
    Bytes charServer(17, 0);
    wr16(charServer, 0, 0x0065);
    CHECK(!parseWantToConnection(charServer).has_value());
    CHECK(!parseWantToConnection(Bytes{0x9b, 0x00}).has_value());
}

TEST(c2s_parse_hotkey_change) {
    Bytes b(11, 0);
    wr16(b, 0, 0x02ba);
    wr16(b, 2, 9);
    b[4] = 1;
    wr32(b, 5, 28);  // Heal
    wr16(b, 9, 10);
    auto c = parseHotkeyChange(b);
    CHECK(c.has_value());
    CHECK(c && c->index == 9);
    CHECK(c && c->key == (Hotkey{1, 28, 10}));
}

TEST(c2s_name_request_and_stop_attack) {
    Bytes n = buildNameRequest(110000123);
    CHECK_EQ(int(n.size()), c2sLength(0x008c));
    CHECK_EQ(rd16(n.data(), 0), 0x008c);
    CHECK_EQ(rd32(n.data(), 7), 110000123u);
    Bytes s = buildStopAttack();
    CHECK_EQ(int(s.size()), c2sLength(0x0118));
    CHECK_EQ(rd16(s.data(), 0), 0x0118);
}

TEST(c2s_want_to_connection_char_id) {
    Bytes b(26, 0);
    wr16(b, 0, 0x009b);
    wr32(b, 4, 2000014);
    wr32(b, 9, 150001);
    CHECK(parseWantToConnectionCharId(b) && *parseWantToConnectionCharId(b) == 150001u);
    CHECK(!parseWantToConnectionCharId(Bytes{0x9b, 0x00}));
}
