#include "core/merc_packet.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::mercpacket {
namespace {

constexpr int32_t kSomeTime = 1000000000;  // 2001: any contract's end is later

int32_t get32(const uint8_t* p, size_t at) {
    int32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}
int16_t get16(const uint8_t* p, size_t at) {
    int16_t v = 0;
    std::memcpy(&v, p + at, 2);
    return v;
}
void put32(uint8_t* p, size_t at, int32_t v) { std::memcpy(p + at, &v, 4); }
void put16(uint8_t* p, size_t at, int32_t v) {
    const int16_t s = int16_t(std::clamp<int32_t>(v, -32768, 32767));
    std::memcpy(p + at, &s, 2);
}

}  // namespace

bool laidOut(uint8_t* p, size_t size) {
    if (!p || size < kInitSize || uint16_t(get16(p, 0)) != kInit) return false;
    if (get32(p, 64) < kSomeTime || get32(p, 56) >= kSomeTime) return false;  // the game's layout already
    const int32_t hp = get32(p, 48), hpMost = get32(p, 52), sp = get32(p, 56), spMost = get32(p, 60);
    const int32_t ends = get32(p, 64), loyalty = get16(p, 68), summons = uint16_t(get16(p, 70));
    put16(p, 48, hp);
    put16(p, 50, hpMost);
    put16(p, 52, sp);
    put16(p, 54, spMost);
    put32(p, 56, ends);
    put16(p, 60, loyalty);
    put32(p, 62, summons);
    put32(p, 66, 0);  // kills: past the 72 bytes
    put16(p, 70, 0);  // range: past them too
    return true;
}

std::vector<Skill> skills(const uint8_t* p, size_t size) {
    std::vector<Skill> out;
    if (!p || size < 4 || uint16_t(get16(p, 0)) != kSkills) return out;
    const size_t len = std::min(size, size_t(uint16_t(get16(p, 2))));
    for (size_t at = 4; at + 37 <= len; at += 37) {
        Skill k;
        k.id = uint16_t(get16(p, at));
        k.lv = get16(p, at + 6);
        k.sp = get16(p, at + 8);
        k.range = get16(p, at + 10);
        if (k.id) out.push_back(k);
    }
    return out;
}

std::string skillName(int id) {
    static const char* const kNames[] = {
        "Bash",           "Magnum Break",     "Bowling Bash",     "Parry",          "Shield Reflect",
        "Frenzy",         "Double Strafe",    "Arrow Shower",     "Skid Trap",      "Land Mine",
        "Sandman",        "Freezing Trap",    "Remove Trap",      "Arrow Repel",    "Focused Arrow Strike",
        "Pierce",         "Brandish Spear",   "Spiral Pierce",    "Defending Aura", "Guard",
        "Sacrifice",      "Magnificat",       "Two-Hand Quicken", "Sight",          "Crash",
        "Regain",         "Tender",           "Benediction",      "Recuperate",     "Mental Cure",
        "Compress",       "Provoke",          "Berserk",          "Decrease AGI",   "Scapegoat",
        "Lex Divina",     "Sense",            "Kyrie Eleison",    "Blessing",       "Increase Agility"};
    constexpr int kFirst = 8201;
    const int i = id - kFirst;
    return i >= 0 && i < int(sizeof kNames / sizeof kNames[0]) ? kNames[i] : "";
}

}  // namespace uagfx::mercpacket
