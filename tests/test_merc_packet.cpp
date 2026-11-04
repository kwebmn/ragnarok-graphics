// 3.0.11-beta.218 (R-S8BERR: the mercenary card «HP 256 / 0», «Викликів 359399424», «Контракт до 01.01 02:03»): the
// uaRO server fills ZC_MER_INIT (0x029b, 72 bytes) in the later layout - HP, its most, SP, its most as ints from 48,
// the contract's end at 64, loyalty 68, summons 70 - while uaRO.exe (FUN_0059b950) reads the 2007 one: shorts at 48,
// 50, 52, 54, the end at 56, loyalty 60, summons 62, kills 66, range 70. Laid out again in place before the game reads.
#include <cstdint>
#include <cstring>
#include <vector>

#include "core/merc_packet.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void w16(std::vector<uint8_t>& p, size_t at, int v) {
    const int16_t x = int16_t(v);
    std::memcpy(p.data() + at, &x, 2);
}
void w32(std::vector<uint8_t>& p, size_t at, int64_t v) {
    const int32_t x = int32_t(v);
    std::memcpy(p.data() + at, &x, 4);
}
int r16(const std::vector<uint8_t>& p, size_t at) {
    int16_t x = 0;
    std::memcpy(&x, p.data() + at, 2);
    return x;
}
int32_t r32(const std::vector<uint8_t>& p, size_t at) {
    int32_t x = 0;
    std::memcpy(&x, p.data() + at, 4);
    return x;
}

std::vector<uint8_t> serverInit() {  // as uAthena's clif_mercenary_info fills it, cut at 72
    std::vector<uint8_t> p(72, 0);
    w16(p, 0, 0x29b);
    w32(p, 2, 110000123);
    w16(p, 6, 250);  // ATK ... the stats as both layouts have them
    std::memcpy(p.data() + 22, "Mina", 4);
    w16(p, 46, 20);
    w32(p, 48, 2256);   // HP
    w32(p, 52, 2300);   // its most
    w32(p, 56, 80);     // SP
    w32(p, 60, 100);    // its most
    w32(p, 64, 1762255200);  // the contract's end
    w16(p, 68, 200);    // loyalty
    w16(p, 70, 3);      // summons (an int cut to its low half by the 72 bytes)
    return p;
}

}  // namespace

TEST(merc_packet_server_layout_laid_out_for_the_game) {
    std::vector<uint8_t> p = serverInit();
    CHECK(mercpacket::laidOut(p.data(), p.size()));
    CHECK_EQ(r16(p, 46), 20);
    CHECK_EQ(r16(p, 48), 2256);
    CHECK_EQ(r16(p, 50), 2300);
    CHECK_EQ(r16(p, 52), 80);
    CHECK_EQ(r16(p, 54), 100);
    CHECK_EQ(r32(p, 56), 1762255200);
    CHECK_EQ(r16(p, 60), 200);
    CHECK_EQ(r32(p, 62), 3);
    CHECK_EQ(r32(p, 66), 0);  // kills: not in the 72 bytes
    CHECK_EQ(r16(p, 6), 250);
    CHECK(std::memcmp(p.data() + 22, "Mina", 4) == 0);
    CHECK(!mercpacket::laidOut(p.data(), p.size()));  // the game's layout now: left alone
}

TEST(merc_packet_game_layout_and_others_left_alone) {
    std::vector<uint8_t> p(72, 0);  // the 2007 layout
    w16(p, 0, 0x29b);
    w16(p, 48, 900);
    w16(p, 50, 1000);
    w16(p, 52, 50);
    w16(p, 54, 60);
    w32(p, 56, 1762255200);
    const std::vector<uint8_t> before = p;
    CHECK(!mercpacket::laidOut(p.data(), p.size()));
    CHECK(p == before);
    std::vector<uint8_t> big = serverInit();
    w32(big, 48, 40000);  // past a short: held at its most
    w32(big, 52, 45000);
    CHECK(mercpacket::laidOut(big.data(), big.size()));
    CHECK(r16(big, 48) == 32767 && r16(big, 50) == 32767);
    std::vector<uint8_t> shortOne = serverInit();
    CHECK(!mercpacket::laidOut(shortOne.data(), 60));  // not the whole packet
}

// 3.0.11-beta.218 (R-S8BERR: «блок со скилами наемника пустой»): the game names skills only below 0x3FD (its table at
// 0x74B9AC), the mercenaries' are 8201-8240 - their list read from ZC_MER_SKILLINFO_LIST (0x029d: 37 bytes each - the
// id, its kind, level, SP, range, its Aegis name, upgradable) and named by the driver
TEST(merc_packet_skill_list_read_and_named) {
    std::vector<uint8_t> p(4 + 2 * 37, 0);
    w16(p, 0, 0x29d);
    w16(p, 2, int(p.size()));
    w16(p, 4, 8207);
    w16(p, 4 + 6, 2);
    w16(p, 4 + 8, 12);
    std::memcpy(p.data() + 4 + 12, "MA_DOUBLE", 9);
    w16(p, 41, 8233);
    w16(p, 41 + 6, 1);
    const std::vector<mercpacket::Skill> s = mercpacket::skills(p.data(), p.size());
    CHECK_EQ(s.size(), size_t(2));
    if (s.size() == 2) {
        CHECK(s[0].id == 8207 && s[0].lv == 2 && s[0].sp == 12);
        CHECK(s[1].id == 8233 && s[1].lv == 1);
    }
    CHECK_EQ(mercpacket::skillName(8207), std::string("Double Strafe"));
    CHECK_EQ(mercpacket::skillName(8223), std::string("Two-Hand Quicken"));
    CHECK_EQ(mercpacket::skillName(8240), std::string("Increase Agility"));
    CHECK_EQ(mercpacket::skillName(9000), std::string());
    CHECK(mercpacket::skills(p.data(), 20).empty());  // cut short
}
