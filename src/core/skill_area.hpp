#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::skillarea {

// 3.0.11-beta.142 (the owner's concept «маг, прист и хант на геймпаде», Claude Docs YVYxW5sE4kH47HKR3PLc8P): the
// cells a ground skill makes on the uaRO server, by its own rules (uAthena: map.c map_calc_dir; skill.c
// skill_get_unit_layout, skill_init_unit_layout; db/skill_unit_db.txt, db/skill_db.txt) - for the pad's placing and
// the cells shown on the ground before and after a cast
struct Cell {
    int x = 0, y = 0;
};
inline bool operator==(const Cell& a, const Cell& b) { return a.x == b.x && a.y == b.y; }

// The side from (0, 0) toward (dx, dy) as the server tells it: 0 up (+y), 1 up-left, 2 left, 3 down-left, 4 down,
// 5 down-right, 6 right, 7 up-right; the diagonal between about 27 and 63 degrees (its integer rule)
int dirOf(int dx, int dy);
Cell step(int dir);  // one cell that way

// The cells `skill` at `lv` makes, cast from `caster` at `at`: a wall's by the side it is cast to (Fire Wall 3 across
// a straight side, a 5-cell staircase across a diagonal; Ice Wall 5), a zone's shape (Sanctuary 21, Magnus 33,
// Pneuma 3x3...), an area's square where one hidden unit hits about it (Storm Gust 11x11, Lord of Vermilion 13x13,
// Thunder Storm 5x5...), a trap's one; none for a skill not on the ground or not known
std::vector<Cell> cells(uint16_t skill, int lv, Cell caster, Cell at);

// How a press places it (the concept, answers 3, 5, 6): a wall between the caster and the target; an area where it
// covers the most monsters; a zone under oneself (aimed: on a friend); a trap, or Fire Pillar, toward the target;
// the rest on the target's cell
enum class Kind : uint8_t { Other, Wall, Crowd, Friend, Trap, Ahead };
Kind kindOf(uint16_t skill);
int crowdRadius(uint16_t skill);  // a crowd skill's reach about its cell (the monsters a press counts); 0 others
int most(uint16_t skill);         // at once at most (maxcount): Fire Wall 3, Quagmire 3, Fire Pillar 5; 0 no limit

}  // namespace uagfx::skillarea
