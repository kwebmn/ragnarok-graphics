#pragma once
#include <cstdint>

namespace uagfx::skillinfo {

// 3.0.11-beta.223 (the skills concept, stage 1: the skills the server has not sent): what a skill of the server's tree
// is used on and how it is learned - db/skill_db.txt (tools/skill_info.py writes core/skill_info.inc). The client
// knows these for the skills it was sent only (0x010f).
struct Info {
    uint16_t id;
    uint8_t inf;   // the game's: 0 passive, 1 an enemy, 2 the ground, 4 itself, 16 a friend, 32 a trap
    uint8_t inf2;  // kQuest, kWedding, kSoulLink
};
constexpr uint8_t kQuest = 1, kWedding = 4, kSoulLink = 8;

int type(int id);  // -1: not in the tree
bool quest(int id);
bool wedding(int id);
bool soulLink(int id);

}  // namespace uagfx::skillinfo
