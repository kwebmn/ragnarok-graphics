#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::guildskills {

// 3.0.22-beta.26 (docs/superpowers/plans/2026-11-19-guild-window.md): uaRO's guild skills as its server has them
// (db/guild_skill_tree.txt, skill_db.txt, skill_cast_db.txt; conf/battle guild_aura 31, emergency_call 11 - read on
// kozak 19.11): the window's order and groups, each one's most and needs; Guild's Glory (max 0 there) never shown
enum class Kind : uint8_t { Passive, Aura, Active };
enum class Group : uint8_t { Castle, Growth, Auras, War };
constexpr int kGroups = 4;
struct Need {
    int id = 0, level = 0;
};
struct Def {
    int id = 0;
    const char* aegis = "";  // the icon: data\texture\유저인터페이스\item\<aegis in small letters>.bmp
    const char* name = "";   // as the server names it
    int most = 0;
    Kind kind = Kind::Passive;
    Group group = Group::Castle;
    std::vector<Need> needs;
};
const std::vector<Def>& table();  // the 14 shown, in the window's order
const Def* def(int id);
int indexOf(int id);  // in table(); -1 not there
constexpr int kGlory = 10005;
// Battle Orders, Regeneration, Restoration, Urgent Call: one block for all four after any of them (guild_block_skill,
// skill_cast_db's 300000)
constexpr int kCooldownMs = 300000;
bool shared(int id);

struct Sent {  // a skill as the server sent it (0x0162)
    int id = 0, level = 0, inf = 0, sp = 0;
    bool up = false;  // the server's «can rise»: the master's, below its most (the points not asked)
};
struct Row {
    int id = 0, level = 0, most = 0;
    Kind kind = Kind::Passive;
    Group group = Group::Castle;
    bool locked = false;        // not sent and its needs unmet
    std::vector<Need> missing;  // the needs unmet (for «Потрібно: …»)
    bool plus = false;          // «+»: the master, points left, not locked, below its most (the server's «up» if sent)
    bool usable = false;        // «Використати»: the master, an active one learned
    int coolMs = 0;             // the shared cooldown left (an active one learned)
};
// Every skill of table(): its level as sent (0 when not), locked when not sent and its needs (against the levels sent)
// unmet
std::vector<Row> rows(const std::vector<Sent>& sent, int points, bool master, int coolMs);
int coolLeft(uint64_t usedAt, uint64_t now);  // after a use at `usedAt` (0: none), ms
std::string clock(int ms);                    // «3:12», rounded up

// The side panel's level lines: the auras' «+N STR», Regeneration's three, Battle Orders' and Restoration's one,
// Guild Extension's places now and at the next level (`places`: «{1} місць»); the others none
struct LevelLine {
    int level = 0;
    std::string what;
    bool now = false;
};
std::vector<LevelLine> levels(int id, int level, const std::string& places);
std::string auraBonus(int id, int level);  // an aura's «+N STR» at `level`; "" not an aura or not learned
int places(int extension);                 // 16 + 6 a level of Guild Extension

}  // namespace uagfx::guildskills
