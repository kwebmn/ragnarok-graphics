#pragma once
#include <cstdint>
#include <vector>

#include "core/ground_units.hpp"

namespace uagfx::groundtimers {

// 3.0.16-beta.21 (the owner's pick 7 of «20 идей»: time rings on the ground): [interface] ground_timers - the
// character's own Safety Wall, Pneuma, Sanctuary and traps each with a ring on the ground running down as its time
// does. Their units (0x011f, core/ground_units.hpp) by kind, their time by the skill's level of the cast just asked
// (0x0113) - the server's db/skill_cast_db.txt Duration1; a unit gone (0x0120: a wall broken, a trap sprung) takes
// its ring away. Units seen with no cast of theirs just asked (come back into sight) have no ring: their start is not
// known.
uint16_t skillOfUnit(uint8_t type);           // 0: none of these
int durationMs(uint16_t skill, int level);    // 0: not known
constexpr uint64_t kCastWaitMs = 3000;        // a cast asked this long before its units come
constexpr uint64_t kTogetherMs = 150;         // a cast's units come together

struct Ring {
    uint16_t skill = 0;
    float x = 0, y = 0;   // its cells' middle (cells, a cell's middle at +0.5)
    float radius = 0.5f;  // cells
    uint64_t startMs = 0, endMs = 0;
    float left(uint64_t nowMs) const;  // 1 at the start, 0 at the end
};

class Timers {
public:
    void asked(uint16_t skill, int level, uint64_t nowMs);  // the character's cast (0x0113)
    void seen(const groundunits::Seen& s, uint32_t self, uint64_t nowMs);
    void gone(uint32_t unit);
    void clear();  // a new map
    std::vector<Ring> rings(uint64_t nowMs) const;  // the casts with a unit still standing, their time not over

private:
    struct Ask {
        uint16_t skill;
        int level;
        uint64_t atMs;
    };
    struct Unit {
        uint32_t id;
        int cast;
        int x, y;
    };
    struct Cast {
        int id;
        uint16_t skill;
        uint64_t startMs, endMs;
    };
    std::vector<Ask> asks_;
    std::vector<Unit> units_;
    std::vector<Cast> casts_;
    int next_ = 1;
};

}  // namespace uagfx::groundtimers
