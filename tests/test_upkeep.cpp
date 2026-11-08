// 3.0.16-beta.13 (the owner's picks 19, 20 of «20 идей»): broken gear, the pet's and the homunculus's fullness
#include <cstring>
#include <vector>

#include "core/upkeep.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
std::vector<uint8_t> equipList(std::vector<std::pair<int, int>> pieces) {  // item, attribute
    std::vector<uint8_t> p(4 + pieces.size() * 20, 0);
    p[0] = 0xa4;
    p[2] = uint8_t(p.size());
    for (size_t i = 0; i < pieces.size(); ++i) {
        p[4 + i * 20 + 2] = uint8_t(pieces[i].first & 0xFF);
        p[4 + i * 20 + 3] = uint8_t(pieces[i].first >> 8);
        p[4 + i * 20 + 10] = uint8_t(pieces[i].second);
    }
    return p;
}
}  // namespace

TEST(upkeep_broken_gear) {
    upkeep::Watch w;
    auto a = equipList({{1201, 0}, {2301, 1}});  // the login's: a broken coat already
    w.packet(0x00a4, a.data(), a.size());
    CHECK(w.broken().count(2301) == 1);
    CHECK(w.takeFresh().empty());  // known before: no chime
    auto b = equipList({{1201, 1}, {2301, 1}});  // the knife broke
    CHECK(w.packet(0x00a4, b.data(), b.size()));
    const std::vector<int> fresh = w.takeFresh();
    CHECK(fresh.size() == 1 && fresh[0] == 1201);
    auto c = equipList({{1201, 0}, {2301, 0}});  // repaired
    w.packet(0x00a4, c.data(), c.size());
    CHECK(w.broken().empty());
}

TEST(upkeep_pet_and_homunculus) {
    upkeep::Watch w;
    uint8_t status[35] = {0xa2, 0x01};
    status[29] = 60;
    w.packet(0x01a2, status, sizeof status);
    CHECK_EQ(w.pet(), 60);
    uint8_t data[11] = {0xa4, 0x01, 2};
    data[7] = 24;
    w.packet(0x01a4, data, sizeof data);
    CHECK_EQ(w.pet(), 24);
    CHECK_EQ(upkeep::hungerLevel(24), 1);
    uint8_t info[71] = {0x2e, 0x02};
    info[29] = 9;
    w.packet(0x022e, info, sizeof info);
    CHECK_EQ(w.homun(), 9);
    CHECK_EQ(upkeep::hungerLevel(9), 2);
    info[26] = 2;  // put to rest
    w.packet(0x022e, info, sizeof info);
    CHECK_EQ(w.homun(), -1);
    uint8_t move[22] = {0x91, 0x00};
    w.packet(0x0091, move, sizeof move);
    CHECK_EQ(w.pet(), -1);
}
