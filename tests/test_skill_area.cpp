// 3.0.11-beta.142 (the owner's concept «маг, прист и хант на геймпаде», answers of 2026-11-01): the server's own cells
// of a ground skill (core/skill_area) - its direction rule (uAthena map_calc_dir), Fire Wall's and Ice Wall's shapes by
// the side they are cast to (skill_init_unit_layout), the zones' and areas' sizes (db/skill_unit_db.txt), and which
// kind of placing each skill takes
#include <algorithm>
#include <vector>

#include "core/skill_area.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

using C = skillarea::Cell;

bool same(std::vector<C> a, std::vector<C> b) {
    const auto less = [](const C& p, const C& q) { return p.x != q.x ? p.x < q.x : p.y < q.y; };
    std::sort(a.begin(), a.end(), less);
    std::sort(b.begin(), b.end(), less);
    return a == b;
}

}  // namespace

// map_calc_dir: 0 up (+y), 1 up-left, 2 left, 3 down-left, 4 down, 5 down-right, 6 right, 7 up-right; the diagonal
// between about 27 and 63 degrees, as the server's integer rule draws it
TEST(skill_area_direction_as_the_server) {
    CHECK(skillarea::dirOf(0, 3) == 0 && skillarea::dirOf(-3, 0) == 2 && skillarea::dirOf(0, -3) == 4);
    CHECK(skillarea::dirOf(3, 0) == 6 && skillarea::dirOf(3, 3) == 7 && skillarea::dirOf(-3, 3) == 1);
    CHECK(skillarea::dirOf(-3, -3) == 3 && skillarea::dirOf(3, -3) == 5);
    CHECK(skillarea::dirOf(2, 1) == 7 && skillarea::dirOf(3, 1) == 6 && skillarea::dirOf(1, 2) == 0);
    CHECK(skillarea::dirOf(1, 1) == 7 && skillarea::dirOf(-1, -3) == 4 && skillarea::dirOf(-2, -3) == 3);
}

// Fire Wall: 3 cells across a straight side, a 5-cell staircase across a diagonal one; on the caster's own cell as
// the side «left» (a wall up and down)
TEST(skill_area_fire_wall_by_side) {
    const C me{50, 50};
    CHECK(same(skillarea::cells(18, 10, me, C{50, 53}), {{49, 53}, {50, 53}, {51, 53}}));
    CHECK(same(skillarea::cells(18, 10, me, C{53, 50}), {{53, 49}, {53, 50}, {53, 51}}));
    CHECK(same(skillarea::cells(18, 10, me, C{53, 53}), {{52, 54}, {52, 53}, {53, 53}, {53, 52}, {54, 52}}));
    CHECK(same(skillarea::cells(18, 10, me, C{47, 53}), {{48, 54}, {48, 53}, {47, 53}, {47, 52}, {46, 52}}));
    CHECK(same(skillarea::cells(18, 10, me, C{50, 50}), {{50, 49}, {50, 50}, {50, 51}}));
}

// Ice Wall: always 5 - a straight line across, or a straight diagonal
TEST(skill_area_ice_wall_always_five) {
    const C me{50, 50};
    CHECK(same(skillarea::cells(87, 10, me, C{50, 53}), {{48, 53}, {49, 53}, {50, 53}, {51, 53}, {52, 53}}));
    CHECK(same(skillarea::cells(87, 10, me, C{53, 53}), {{51, 55}, {52, 54}, {53, 53}, {54, 52}, {55, 51}}));
}

// The zones and the areas, by the server's table: one cell, 3x3, Sanctuary's 21, Magnus' 33, 5x5, Storm Gust's 11x11
TEST(skill_area_zones_and_areas) {
    const C me{50, 50}, at{60, 60};
    CHECK(skillarea::cells(12, 10, me, at).size() == 1);    // Safety Wall
    CHECK(skillarea::cells(25, 1, me, at).size() == 9);     // Pneuma
    CHECK(skillarea::cells(70, 10, me, at).size() == 21);   // Sanctuary
    CHECK(skillarea::cells(79, 10, me, at).size() == 33);   // Magnus Exorcismus
    CHECK(skillarea::cells(92, 5, me, at).size() == 25);    // Quagmire
    CHECK(skillarea::cells(21, 10, me, at).size() == 25);   // Thunder Storm
    CHECK(skillarea::cells(89, 10, me, at).size() == 121);  // Storm Gust
    CHECK(skillarea::cells(85, 10, me, at).size() == 169);  // Lord of Vermilion
    CHECK(skillarea::cells(117, 5, me, at).size() == 1);    // Ankle Snare
    CHECK(skillarea::cells(19, 10, me, at).empty());        // Fire Bolt: not on the ground
    const std::vector<C> sanctuary = skillarea::cells(70, 10, me, at);
    CHECK(std::count(sanctuary.begin(), sanctuary.end(), C{58, 58}) == 0);  // no corners
    CHECK(std::count(sanctuary.begin(), sanctuary.end(), C{60, 62}) == 1);
}

// How each is placed by a press: a wall between, the crowd's best cell, a zone under oneself, a trap toward the target
TEST(skill_area_kinds) {
    using K = skillarea::Kind;
    CHECK(skillarea::kindOf(18) == K::Wall && skillarea::kindOf(87) == K::Wall);
    CHECK(skillarea::kindOf(89) == K::Crowd && skillarea::kindOf(85) == K::Crowd && skillarea::kindOf(79) == K::Crowd);
    CHECK(skillarea::kindOf(70) == K::Friend && skillarea::kindOf(25) == K::Friend &&
          skillarea::kindOf(12) == K::Friend);
    CHECK(skillarea::kindOf(117) == K::Trap && skillarea::kindOf(123) == K::Trap);
    CHECK(skillarea::kindOf(80) == K::Ahead);
    CHECK(skillarea::kindOf(92) == K::Other && skillarea::kindOf(19) == K::Other);
    CHECK(skillarea::crowdRadius(89) == 5 && skillarea::crowdRadius(21) == 2 && skillarea::crowdRadius(19) == 0);
    CHECK(skillarea::most(18) == 3 && skillarea::most(92) == 3 && skillarea::most(80) == 5 && skillarea::most(87) == 0);
}
