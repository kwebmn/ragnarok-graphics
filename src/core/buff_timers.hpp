#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace uagfx::buffs {

// 3.0.4-beta.6 (the user's idea 4): the player's buffs counted down. uaRO.exe (2007) shows a buff's icon with no time
// (ZC_MSG_STATE_CHANGE 0x0196 has none); the server's own data has it (core/buff_table.hpp). The skill cast on the
// character (0x011a, another player's too) gives its level; without one - an item, or a cast missed - the longest
// time the table has for the icon. An icon the server sends again (a map change) keeps its time; one put out ends.
uint32_t duration(uint16_t icon, uint16_t skill, uint16_t level);  // ms; 0 none known

class Timers {
public:
    static constexpr uint64_t kCastWindowMs = 3000;  // a cast this close before its icon gave it
    void cast(uint16_t skill, uint16_t level, uint64_t now);
    void icon(uint16_t icon, bool on, uint64_t now);
    uint32_t left(uint16_t icon, uint64_t now) const;  // ms; 0 none or ended
    void clear() { ends_.clear(); }

private:
    struct Cast {
        bool known = false;
        uint16_t skill = 0, level = 0;
        uint64_t at = 0;
    };
    Cast last_;
    std::map<uint16_t, uint64_t> ends_;  // icon -> when it ends
};

// What an icon shows of the time left: "45" (seconds), "4:32" (under ten minutes), "29m"; "" none
std::string label(uint32_t ms);

}  // namespace uagfx::buffs
