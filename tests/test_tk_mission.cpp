// 3.0.22-beta.4 (the owner's «ок» to idea 3; R-JGEFVM: «в тейквон місію назначило реневал моба, якого немає на
// сервері - Solid Antique Book (C2_ANTIQUE_BOOK) Mob-ID#2898»): the Taekwon mission's monster (the server's 0x020E,
// clif_mission_info) told in the chat with the maps it lives on - or that it lives nowhere here (core/tk_mission)
#include <cstring>
#include <string>
#include <vector>

#include "core/reference_index.hpp"
#include "core/settings/strings.hpp"
#include "core/tk_mission.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
std::vector<uint8_t> packet(const char* name, int mob, int progress, int kind) {
    std::vector<uint8_t> p(tkmission::kPacketLen, 0);
    p[0] = 0x0E;
    p[1] = 0x02;
    std::memcpy(p.data() + 2, name, std::strlen(name));
    p[26] = uint8_t(mob), p[27] = uint8_t(mob >> 8), p[28] = uint8_t(mob >> 16), p[29] = uint8_t(mob >> 24);
    p[30] = uint8_t(progress);
    p[31] = uint8_t(kind);
    return p;
}
}  // namespace

TEST(tk_mission_read_from_its_packet) {
    tkmission::Mission m;
    const std::vector<uint8_t> p = packet("Poring", 1002, 5, 20);
    CHECK(tkmission::parse(p.data(), p.size(), m));
    CHECK(m.mob == 1002 && m.progress == 5 && m.name == "Poring");
    CHECK(!tkmission::parse(p.data(), p.size() - 1, m));  // cut short
    const std::vector<uint8_t> feel = packet("prontera", 0, 0, 0);  // the Star Gladiator's places: not a mission
    CHECK(!tkmission::parse(feel.data(), feel.size(), m));
}

TEST(tk_mission_where_its_monster_lives) {
    refindex::Index index;
    index.mobs[1002].name = "Poring";
    index.spawnsOf[1002] = {{"prt_fild08", 75, 0}, {"pay_fild01", 40, 0}};
    tkmission::Mission m;
    m.mob = 1002;
    m.name = "Poring";
    CHECK_EQ(tkmission::line(m, &index, settings::Lang::Uk),
             std::string("Місія таеквона: Poring — водиться на prt_fild08 (75), pay_fild01 (40)."));
    index.spawnsOf[1002] = {{"a", 6, 0}, {"b", 5, 0}, {"c", 4, 0}, {"d", 3, 0}, {"e", 2, 0}, {"f", 1, 0}};
    CHECK_EQ(tkmission::line(m, &index, settings::Lang::Uk),
             std::string("Місія таеквона: Poring — водиться на a (6), b (5), c (4), d (3) і ще 2."));
    m.mob = 2898;
    m.name = "Solid Antique Book";
    CHECK_EQ(tkmission::line(m, &index, settings::Lang::Uk),
             std::string("Місія таеквона: Solid Antique Book — на цьому сервері не водиться, тож місію не виконати; "
                         "змінити ціль сервер не дає."));
    CHECK_EQ(tkmission::line(m, nullptr, settings::Lang::Uk), std::string());  // the reference not loaded: nothing
}
