#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uagfx::groundunits {

// 3.0.11-beta.143 (the owner's concept «маг, прист и хант на геймпаде»: how many walls stand, «2/3» on the slot): the
// character's own skill units on the ground from the server's packets (uAthena clif.c) - 0x011f a unit seen (16
// bytes: the unit +2, its caster +6, its cell +10 and +12, its kind +14) and 0x0120 one gone (6 bytes: the unit +2) -
// grouped into casts: a cast's units come together (a Fire Wall's 3 or 5 cells, Quagmire's 25)
struct Seen {
    uint32_t unit = 0, caster = 0;
    int x = 0, y = 0;
    uint8_t type = 0;
};
bool readSeen(const uint8_t* p, size_t n, Seen& out);
bool readGone(const uint8_t* p, size_t n, uint32_t& unit);

// The unit kinds a skill makes (db/skill_unit_db.txt): Fire Wall 0x7f, Ice Wall 0x8d, Quagmire 0x8e, Fire Pillar 0x87
// and 0x88 once it fired; none for the others (not counted)
std::vector<uint8_t> unitTypes(uint16_t skill);

class Tracker {
public:
    // A unit seen: one of the caster's (`self`) of a counted kind joins the cast of its kind begun within 150 ms, or
    // begins one; one known already (back in sight) changes nothing
    void seen(const Seen& s, uint32_t self, uint64_t nowMs);
    void gone(uint32_t unit, uint64_t nowMs);
    void clear();  // a new map
    int standing(uint16_t skill) const;      // its casts with a unit still on the ground
    uint64_t burntOut(uint16_t skill) const;  // when its last cast to go went entirely; 0 none yet

private:
    struct Unit {
        uint32_t id;
        uint16_t skill;
        int cast;
    };
    struct Cast {
        int id;
        uint16_t skill;
        uint64_t atMs;
        int units;
    };
    struct Out {
        uint16_t skill;
        uint64_t atMs;
    };
    std::vector<Unit> units_;
    std::vector<Cast> casts_;
    std::vector<Out> out_;
    int next_ = 1;
};

}  // namespace uagfx::groundunits
