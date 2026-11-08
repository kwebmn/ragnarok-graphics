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

// 3.0.16-beta.36 (R-XQ5XQP): the own pet's unit from a 0x01a4 of another type while the game knows none
TEST(upkeep_own_pet_from_other_packets) {
    uint8_t hunger[11] = {0xa4, 0x01, 2, 0x5f, 0x51, 0x91, 0x06, 95, 0, 0, 0};  // unit 110186847, fullness 95
    CHECK_EQ(upkeep::ownPetOf(0x01a4, hunger, sizeof hunger, 0), 110186847u);
    CHECK_EQ(upkeep::ownPetOf(0x01a4, hunger, sizeof hunger, 5), 0u);  // the game knows one
    hunger[2] = 0;
    CHECK_EQ(upkeep::ownPetOf(0x01a4, hunger, sizeof hunger, 0), 0u);  // type 0: the game's own way
    uint8_t spawn[22] = {0x78, 0x00, 0x5f, 0x51, 0x91, 0x06};
    spawn[14] = 0x05, spawn[15] = 0x04;  // class 1029
    spawn[16] = 100;
    uint32_t unit = 0;
    CHECK_EQ(upkeep::petClassOf(0x0078, spawn, sizeof spawn, unit), 1029);
    CHECK_EQ(unit, 110186847u);
    spawn[16] = 0;
    CHECK_EQ(upkeep::petClassOf(0x0078, spawn, sizeof spawn, unit), 0);
}
