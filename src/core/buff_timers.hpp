#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace uagfx::buffs {

// 3.0.4-beta.6 (the user's idea 4): the player's buffs counted down. uaRO.exe (2007) shows a buff's icon with no time
// (ZC_MSG_STATE_CHANGE 0x0196 has none); the server's own data has it (core/buff_table.hpp). The skill cast on the
// character (0x011a, another player's too) gives its level; without one - an item, or a cast missed - the longest
// time the table has for the icon. An icon the server sends again (a map change) keeps its time; one put out ends.
// 3.0.4-beta.36 (R-C8NYXS «когда каст обновляю отсчет не сбрасывается»): uAthena puts the icon on before it tells the
// cast (clif_skill_nodamage(..., sc_start(...)) - the argument first): a cast just after an icon of its skill put on
// gives that icon its time from the icon's moment - a recast anew, a first cast its level's.
uint32_t duration(uint16_t icon, uint16_t skill, uint16_t level);  // ms; 0 none known

class Timers {
public:
    static constexpr uint64_t kCastWindowMs = 3000;  // a cast this close before its icon gave it
    void cast(uint16_t skill, uint16_t level, uint64_t now);
    void icon(uint16_t icon, bool on, uint64_t now);
    uint32_t left(uint16_t icon, uint64_t now) const;  // ms; 0 none or ended
    void clear() {  // another character: none of the last one's (3.0.4-beta.36: its icons' moments and cast too)
        ends_.clear();
        onAt_.clear();
        last_ = Cast{};
    }

private:
    struct Cast {
        bool known = false;
        uint16_t skill = 0, level = 0;
        uint64_t at = 0;
    };
    Cast last_;
    std::map<uint16_t, uint64_t> ends_;  // icon -> when it ends
    std::map<uint16_t, uint64_t> onAt_;  // 3.0.4-beta.36: icon -> when the server last put it on
};

// What an icon shows of the time left: "45" (seconds), "4:32" (under ten minutes), "29m"; "" none
std::string label(uint32_t ms);

}  // namespace uagfx::buffs
