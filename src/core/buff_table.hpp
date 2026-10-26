#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::buffs {

// 3.0.4-beta.6 (the user's idea 4): how long the server's buffs last - generated from its own data by
// tools/buff_durations.py (core/buff_table_data.cpp): the status icon a skill gives and its time by level (ms, 1-10),
// and an item's (potions, food) icon and the longest time an item gives it.
struct SkillBuff {
    uint16_t icon, skill;
    uint32_t ms[10];
    const char* name;  // 3.0.7-beta.6: the skill's words in skill_db ("Blessing")
};
struct ItemBuff {
    uint16_t icon;
    uint32_t ms;
    const char* name;  // the item's of the longest time ("Concentration Potion")
};
extern const SkillBuff kSkillBuffs[];
extern const size_t kSkillBuffCount;
extern const ItemBuff kItemBuffs[];
extern const size_t kItemBuffCount;

}  // namespace uagfx::buffs
