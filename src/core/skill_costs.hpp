#pragma once
#include <cstdint>

namespace uagfx::skillcost {

// 3.0.11-beta.45 (the hotbar's «Немає SP», the concept's cell states): the SP a skill costs at a level - the server's
// db/skill_require_db.txt (tools/skill_costs.py writes core/skill_costs.inc). The client knows only its learned
// level's price (0x010f); a slot of the hotbar may keep a lower level.
struct SkillCost {
    uint16_t id;
    uint8_t levels;  // the values given; the levels past them cost the last
    uint16_t sp[10];
};
// -1: a skill or a level the table has not
int sp(int id, int level);

}  // namespace uagfx::skillcost
