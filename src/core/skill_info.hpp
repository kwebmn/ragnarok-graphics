#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::skillinfo {

// 3.0.11-beta.223 (the skills concept, stage 1: the skills the server has not sent): what a skill of the server's tree
// is used on and how it is learned - db/skill_db.txt (tools/skill_info.py writes core/skill_info.inc). The client
// knows these for the skills it was sent only (0x010f).
struct Info {
    uint16_t id;
    uint8_t inf;   // the game's: 0 passive, 1 an enemy, 2 the ground, 4 itself, 16 a friend, 32 a trap
    uint8_t inf2;  // kQuest, kWedding, kSoulLink
    const char* name;  // 3.0.11-beta.228: the server's (English)
    // 3.0.18-beta.244 («Довідник» › «Навички»): its AEGIS name, most level, element (its first level's: 0 neutral ..
    // 9 undead, -1 the weapon's, -2 the endowed one, -3 random), kind (weapon, magic, misc, none)
    const char* aegis;
    uint8_t most;
    int8_t element;
    const char* kind;
};
constexpr uint8_t kQuest = 1, kWedding = 4, kSoulLink = 8;

int type(int id);  // -1: not in the tree
bool quest(int id);
bool wedding(int id);
bool soulLink(int id);
const char* name(int id);  // 3.0.11-beta.228: "" none
// 3.0.18-beta.244: the card's facts ("" / 0 none); idOf: a skill by its AEGIS name, 0 none
const char* aegis(int id);
int most(int id);
int element(int id);
const char* kind(int id);
int idOf(const char* aegis);
// Every skill of the tree, by its number
std::vector<int> all();

}  // namespace uagfx::skillinfo
