// 3.0.11-beta.220 (the owner «1-3 делай»: the guild's tabs 2-5 in the new look): the positions (0x0166 names, 0x0160
// rights and tax), the guild's skills (0x0162: the points, each skill's level and whether it can rise), the expelled
// (0x0163: name, account, reason) and the notice (0x016f: subject, body) read from the server's packets
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/guild_tabs.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void w16(std::vector<uint8_t>& p, size_t at, int v) {
    p[at] = uint8_t(v);
    p[at + 1] = uint8_t(v >> 8);
}
void w32(std::vector<uint8_t>& p, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[at + size_t(i)] = uint8_t(v >> (8 * i));
}
void text(std::vector<uint8_t>& p, size_t at, const char* t) { std::memcpy(p.data() + at, t, std::strlen(t)); }

}  // namespace

TEST(guild_tabs_positions_from_both_packets) {
    std::vector<uint8_t> names(4 + 2 * 28, 0);
    w16(names, 0, 0x166);
    w16(names, 2, int(names.size()));
    w32(names, 4, 0);
    text(names, 8, "Master");
    w32(names, 32, 1);
    text(names, 36, "Officer");
    std::vector<uint8_t> infos(4 + 2 * 16, 0);
    w16(infos, 0, 0x160);
    w16(infos, 2, int(infos.size()));
    w32(infos, 4, 0);
    w32(infos, 8, 0x11);  // invite + expel
    w32(infos, 16, 10);   // tax
    w32(infos, 20, 1);
    w32(infos, 24, 0x01);
    w32(infos, 32, 0);
    guildtabs::Data d;
    CHECK(guildtabs::read(d, names.data(), names.size()));
    CHECK(guildtabs::read(d, infos.data(), infos.size()));
    CHECK_EQ(d.positions.size(), size_t(2));
    if (d.positions.size() == 2) {
        CHECK(d.positions[0].name == "Master" && d.positions[0].invite && d.positions[0].expel &&
              d.positions[0].tax == 10);
        CHECK(d.positions[1].name == "Officer" && d.positions[1].invite && !d.positions[1].expel &&
              d.positions[1].tax == 0);
    }
}

TEST(guild_tabs_skills_expelled_notice) {
    std::vector<uint8_t> sk(6 + 2 * 37, 0);
    w16(sk, 0, 0x162);
    w16(sk, 2, int(sk.size()));
    w16(sk, 4, 3);  // points
    w16(sk, 6, 10000);
    w16(sk, 6 + 6, 1);
    sk[6 + 36] = 0;
    w16(sk, 43, 10004);
    w16(sk, 43 + 6, 2);
    sk[43 + 36] = 1;
    guildtabs::Data d;
    CHECK(guildtabs::read(d, sk.data(), sk.size()));
    CHECK_EQ(d.points, 3);
    CHECK_EQ(d.skills.size(), size_t(2));
    if (d.skills.size() == 2) {
        CHECK(d.skills[0].id == 10000 && d.skills[0].lv == 1 && !d.skills[0].up);
        CHECK(d.skills[1].id == 10004 && d.skills[1].lv == 2 && d.skills[1].up);
    }
    CHECK_EQ(guildtabs::skillName(10004), std::string("Guild Extension"));
    CHECK_EQ(guildtabs::skillName(10014), std::string("Permanent Development"));
    CHECK(guildtabs::skillName(9999).empty());
    std::vector<uint8_t> ex(4 + 88, 0);
    w16(ex, 0, 0x163);
    w16(ex, 2, int(ex.size()));
    text(ex, 4, "Bob");
    text(ex, 28, "bob01");
    text(ex, 52, "spam");
    CHECK(guildtabs::read(d, ex.data(), ex.size()));
    CHECK(d.expelled.size() == 1 && d.expelled[0].name == "Bob" && d.expelled[0].account == "bob01" &&
          d.expelled[0].reason == "spam");
    std::vector<uint8_t> no(182, 0);
    w16(no, 0, 0x16f);
    text(no, 2, "Siege tonight");
    text(no, 62, "Meet at Prontera 20:00");
    CHECK(guildtabs::read(d, no.data(), no.size()));
    CHECK(d.subject == "Siege tonight" && d.body == "Meet at Prontera 20:00");
    std::vector<uint8_t> other(10, 0);
    w16(other, 0, 0x99);
    CHECK(!guildtabs::read(d, other.data(), other.size()));
}

// 3.0.22-beta.26 (docs/superpowers/plans/2026-11-19-guild-window.md, task 2): a skill's kind, SP and range (0x0162),
// the alliances (0x014c: opposition 0 an ally, 1 an enemy) and the members as the server lists them (0x0154, 104 bytes
// each: the class, level, the experience given, online, the position)
TEST(guild_tabs_skill_kind_alliances_and_members) {
    std::vector<uint8_t> sk(6 + 37, 0);
    w16(sk, 0, 0x162);
    w16(sk, 2, int(sk.size()));
    w16(sk, 4, 2);
    w16(sk, 6, 10011);
    w16(sk, 8, 4);   // inf: self
    w16(sk, 12, 1);  // level
    w16(sk, 14, 1);  // sp
    w16(sk, 16, 15);
    text(sk, 18, "Regeneration");
    sk[42] = 1;
    guildtabs::Data d;
    CHECK(guildtabs::read(d, sk.data(), sk.size()));
    CHECK_EQ(d.skills.size(), size_t(1));
    if (!d.skills.empty()) {
        CHECK_EQ(d.skills[0].inf, 4);
        CHECK_EQ(d.skills[0].sp, 1);
        CHECK_EQ(d.skills[0].range, 15);
        CHECK(d.skills[0].up);
    }
    std::vector<uint8_t> al(4 + 3 * 32, 0);
    w16(al, 0, 0x14c);
    w16(al, 2, int(al.size()));
    w32(al, 4, 0);
    w32(al, 8, 77);
    text(al, 12, "Nightfall");
    w32(al, 36, 1);
    w32(al, 40, 78);
    text(al, 44, "Red Lotus");
    w32(al, 68, 0);
    w32(al, 72, 79);
    text(al, 76, "Iron Wolves");
    CHECK(guildtabs::read(d, al.data(), al.size()));
    CHECK_EQ(d.alliances.size(), size_t(3));
    CHECK(d.alliancesSeen);
    if (d.alliances.size() == 3) {
        CHECK(!d.alliances[0].enemy && d.alliances[0].guild == 77 && d.alliances[0].name == "Nightfall");
        CHECK(d.alliances[1].enemy && d.alliances[1].name == "Red Lotus");
    }
    std::vector<uint8_t> none(4, 0);  // a guild without alliances: an empty list, still seen
    w16(none, 0, 0x14c);
    w16(none, 2, 4);
    CHECK(guildtabs::read(d, none.data(), none.size()));
    CHECK(d.alliances.empty() && d.alliancesSeen);
    std::vector<uint8_t> ms(4 + 2 * 104, 0);
    w16(ms, 0, 0x154);
    w16(ms, 2, int(ms.size()));
    w32(ms, 4, 2000001);  // account
    w32(ms, 8, 150001);   // character
    w16(ms, 18, 4008);    // Lord Knight
    w16(ms, 20, 99);
    w32(ms, 22, 4294967295u);  // the server's counter at its end
    w32(ms, 26, 1);            // online
    w32(ms, 30, 0);            // position 0: the master's
    text(ms, 84, "Kael");
    w32(ms, 108, 2000002);
    w32(ms, 112, 150002);
    w16(ms, 122, 18);
    w16(ms, 124, 71);
    w32(ms, 126, 1200);
    w32(ms, 130, 0);
    w32(ms, 134, 3);
    text(ms, 188, "Zlata");
    CHECK(guildtabs::read(d, ms.data(), ms.size()));
    CHECK(d.membersSeen);
    CHECK_EQ(d.members.size(), size_t(2));
    if (d.members.size() == 2) {
        CHECK(d.members[0].account == 2000001 && d.members[0].character == 150001);
        CHECK(d.members[0].job == 4008 && d.members[0].level == 99 && d.members[0].online);
        CHECK_EQ(d.members[0].exp, 4294967295u);
        CHECK_EQ(d.members[0].position, 0);
        CHECK_EQ(d.members[0].name, std::string("Kael"));
        CHECK(!d.members[1].online && d.members[1].position == 3 && d.members[1].exp == 1200u);
    }
}
