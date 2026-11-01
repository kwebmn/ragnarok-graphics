// 3.0.11-beta.143 (the owner's concept «маг, прист и хант на геймпаде»): the character's own skill units on the ground
// (core/ground_units) from the server's 0x011f and 0x0120, grouped into casts - how many Fire Walls stand, when the
// last of them burnt out
#include <cstdint>
#include <vector>

#include "core/ground_units.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::vector<uint8_t> seenPacket(uint32_t unit, uint32_t caster, int x, int y, uint8_t type) {
    std::vector<uint8_t> p(16, 0);
    p[0] = 0x1F;
    p[1] = 0x01;
    for (int i = 0; i < 4; ++i) p[size_t(2 + i)] = uint8_t(unit >> (8 * i));
    for (int i = 0; i < 4; ++i) p[size_t(6 + i)] = uint8_t(caster >> (8 * i));
    p[10] = uint8_t(x);
    p[11] = uint8_t(x >> 8);
    p[12] = uint8_t(y);
    p[13] = uint8_t(y >> 8);
    p[14] = type;
    return p;
}

}  // namespace

TEST(ground_units_read_the_packets) {
    groundunits::Seen s;
    const std::vector<uint8_t> p = seenPacket(5000123, 2000000, 301, 85, 0x7F);
    CHECK(groundunits::readSeen(p.data(), p.size(), s));
    CHECK(s.unit == 5000123 && s.caster == 2000000 && s.x == 301 && s.y == 85 && s.type == 0x7F);
    CHECK(!groundunits::readSeen(p.data(), 15, s));
    const uint8_t gone[6] = {0x20, 0x01, 0xBB, 0x4B, 0x4C, 0x00};  // 5000123 = 0x4C4BBB
    uint32_t unit = 0;
    CHECK(groundunits::readGone(gone, 6, unit) && unit == 5000123);
    CHECK(!groundunits::readGone(p.data(), 6, unit));  // not 0x0120
}

// A Fire Wall's 3 or 5 cells come together: one cast; another 2 s later: two; another's walls not counted; a cast
// stands while one of its cells does - its last one gone, it burnt out (the moment kept for the slot's flash)
TEST(ground_units_fire_walls_standing) {
    groundunits::Tracker t;
    const uint32_t me = 2000000;
    for (uint32_t i = 0; i < 3; ++i) t.seen(groundunits::Seen{100 + i, me, 50 + int(i), 60, 0x7F}, me, 1000 + i * 5);
    for (uint32_t i = 0; i < 5; ++i) t.seen(groundunits::Seen{200 + i, me, 40 + int(i), 60, 0x7F}, me, 3000 + i * 5);
    t.seen(groundunits::Seen{300, 2000001, 70, 60, 0x7F}, me, 3100);  // another's wall
    CHECK(t.standing(18) == 2 && t.burntOut(18) == 0);
    t.seen(groundunits::Seen{100, me, 50, 60, 0x7F}, me, 3200);  // seen again (back in sight): the same
    CHECK(t.standing(18) == 2);
    t.gone(100, 4000);
    t.gone(101, 4100);
    CHECK(t.standing(18) == 2);
    t.gone(102, 4200);  // the first wall's last cell
    CHECK(t.standing(18) == 1 && t.burntOut(18) == 4200);
    t.clear();
    CHECK(t.standing(18) == 0);
}

// Fire Pillar - each cast one unit, 0x87 waiting and 0x88 once it fired; Quagmire's 25 one cast; traps not counted here
TEST(ground_units_pillars_and_quagmire) {
    groundunits::Tracker t;
    const uint32_t me = 2000000;
    t.seen(groundunits::Seen{1, me, 10, 10, 0x87}, me, 1000);
    t.seen(groundunits::Seen{2, me, 12, 10, 0x88}, me, 2000);
    for (uint32_t i = 0; i < 25; ++i) t.seen(groundunits::Seen{10 + i, me, int(i % 5), int(i / 5), 0x8E}, me, 5000);
    CHECK(t.standing(80) == 2 && t.standing(92) == 1);
    CHECK(groundunits::unitTypes(18) == std::vector<uint8_t>{0x7F});
    CHECK(groundunits::unitTypes(19).empty());
}
