// 3.0.22-beta.26 (docs/superpowers/plans/2026-11-19-guild-window.md, task 1): uaRO's guild skills as its server has
// them - the table, the window's rows from what the server sent, the side panel's level lines, the shared cooldown
#include <string>
#include <vector>

#include "core/guild_skills.hpp"
#include "test.hpp"

using namespace uagfx;
namespace gs = uagfx::guildskills;

namespace {
const gs::Row* rowOf(const std::vector<gs::Row>& rows, int id) {
    for (const gs::Row& r : rows)
        if (r.id == id) return &r;
    return nullptr;
}
// The tester's guild (19.11): 2 points; Urgent Call, Cold Heart and Permanent Development not sent
std::vector<gs::Sent> testersGuild(bool master) {
    const int levels[][2] = {{10000, 1}, {10001, 1}, {10002, 1}, {10003, 3}, {10004, 10}, {10005, 0}, {10006, 1},
                             {10007, 0}, {10009, 1}, {10010, 1}, {10011, 1}, {10012, 0}, {10014, 0}};
    std::vector<gs::Sent> out;
    for (const auto& l : levels) {
        const gs::Def* d = gs::def(l[0]);
        const int most = d ? d->most : 0;
        out.push_back(gs::Sent{l[0], l[1], l[0] >= 10010 && l[0] <= 10013 ? 4 : 0, 1, master && l[1] < most});
    }
    return out;
}
}  // namespace

TEST(guild_skills_the_table_is_the_servers_without_glory) {
    const std::vector<gs::Def>& t = gs::table();
    CHECK_EQ(t.size(), size_t(14));
    CHECK(gs::def(gs::kGlory) == nullptr);  // max 0 on the server: never shown
    CHECK_EQ(gs::indexOf(10000), 0);
    CHECK_EQ(gs::indexOf(10014), 5);  // «Розвиток гільдії» after Guild Extension
    CHECK_EQ(std::string(gs::def(10013)->aegis), std::string("GD_EMERGENCYCALL"));
    CHECK_EQ(gs::def(10004)->most, 10);
    CHECK_EQ(gs::def(10011)->most, 3);
    CHECK(gs::def(10008)->kind == gs::Kind::Aura);
    CHECK(gs::def(10012)->kind == gs::Kind::Active);
    CHECK(gs::def(10003)->needs.empty());  // Strengthen Guardians needs nothing on uaRO
    CHECK_EQ(gs::def(10013)->needs.size(), size_t(5));
    int groups[gs::kGroups] = {};
    for (const gs::Def& d : t) ++groups[int(d.group)];
    CHECK_EQ(groups[0], 4);
    CHECK_EQ(groups[1], 2);
    CHECK_EQ(groups[2], 4);
    CHECK_EQ(groups[3], 4);
    CHECK(gs::shared(10010) && gs::shared(10013) && !gs::shared(10009) && !gs::shared(10014));
}

TEST(guild_skills_rows_lock_what_the_server_did_not_send) {
    const std::vector<gs::Row> r = gs::rows(testersGuild(true), 2, true, 0);
    CHECK_EQ(r.size(), size_t(14));
    const gs::Row* cold = rowOf(r, 10008);
    CHECK(cold && cold->locked && cold->missing.size() == 1 && cold->missing[0].id == 10007);
    const gs::Row* call = rowOf(r, 10013);  // not sent, but its needs are met: not locked, level 0
    CHECK(call && !call->locked && call->level == 0);
    const gs::Row* ext = rowOf(r, 10004);
    CHECK(ext && ext->level == 10 && !ext->plus);  // at its most
    const gs::Row* regen = rowOf(r, 10011);
    CHECK(regen && regen->plus && regen->usable);
    CHECK(!rowOf(r, 10012)->usable);  // not learned
    CHECK(!rowOf(r, 10006)->usable);  // an aura: nothing to use
}

TEST(guild_skills_a_member_gets_no_plus_and_no_use) {
    const std::vector<gs::Row> r = gs::rows(testersGuild(false), 2, false, 0);
    for (const gs::Row& k : r) {
        CHECK(!k.plus);
        CHECK(!k.usable);
    }
    const std::vector<gs::Row> none = gs::rows(testersGuild(true), 0, true, 0);  // the master with no points
    CHECK(!rowOf(none, 10011)->plus);
    CHECK(rowOf(none, 10011)->usable);
}

TEST(guild_skills_the_shared_cooldown) {
    CHECK_EQ(gs::coolLeft(0, 5000), 0);
    CHECK_EQ(gs::coolLeft(1000, 1000), gs::kCooldownMs);
    CHECK_EQ(gs::coolLeft(1000, 1000 + 108000), gs::kCooldownMs - 108000);
    CHECK_EQ(gs::coolLeft(1000, 1000 + gs::kCooldownMs + 1), 0);
    CHECK_EQ(gs::clock(192000), std::string("3:12"));
    CHECK_EQ(gs::clock(59001), std::string("1:00"));  // up: «ще 1:00» while a second of it is left
    CHECK_EQ(gs::clock(5000), std::string("0:05"));
    const std::vector<gs::Row> r = gs::rows(testersGuild(true), 2, true, 192000);
    CHECK_EQ(rowOf(r, 10010)->coolMs, 192000);   // learned, active: all four wait
    CHECK_EQ(rowOf(r, 10011)->coolMs, 192000);
    CHECK_EQ(rowOf(r, 10012)->coolMs, 0);        // not learned
    CHECK_EQ(rowOf(r, 10006)->coolMs, 0);        // an aura
}

TEST(guild_skills_level_lines) {
    std::vector<gs::LevelLine> l = gs::levels(10006, 1, "");
    CHECK_EQ(l.size(), size_t(5));
    CHECK_EQ(l[0].what, std::string("+1 STR"));
    CHECK(l[0].now && !l[1].now);
    CHECK_EQ(l[4].what, std::string("+5 STR"));
    l = gs::levels(10011, 1, "");
    CHECK_EQ(l.size(), size_t(3));
    CHECK_EQ(l[0].what, std::string("HP \xC3\x97" "2"));
    CHECK_EQ(l[2].what, std::string("HP \xC3\x97" "3 \xC2\xB7 SP \xC3\x97" "3"));
    l = gs::levels(10004, 10, "{1} places");  // Guild Extension: the places now (and next, while below 10)
    CHECK_EQ(l.size(), size_t(1));
    CHECK_EQ(l[0].what, std::string("76 places"));
    l = gs::levels(10004, 4, "{1} places");
    CHECK_EQ(l.size(), size_t(2));
    CHECK_EQ(l[0].what, std::string("40 places"));
    CHECK(l[0].now);
    CHECK_EQ(l[1].level, 5);
    CHECK_EQ(l[1].what, std::string("46 places"));
    CHECK(gs::levels(10001, 1, "").empty());
    CHECK_EQ(gs::levels(10010, 1, "").size(), size_t(1));
    CHECK_EQ(gs::auraBonus(10008, 3), std::string("+3 AGI"));
    CHECK_EQ(gs::auraBonus(10011, 3), std::string());
}
