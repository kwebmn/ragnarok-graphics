#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace uagfx::refindex {

// 3.0.11-beta.6 (the owner: «1. давай» - the hints on the new minimap): the game's reference pack (core/reference_pack,
// format 1) read for the minimap - per map its NPCs (as the site's map shows them), its warps to other maps and its
// monsters with their counts; the monsters' names. The parts that need no Windows.

enum class NpcKind : uint8_t { Kafra, Healer, Tool, Weapon, Armor, Inn, Guide, Job, Warp, Other };

struct Npc {
    int x = 0, y = 0;
    std::string name;  // as the site cleans it: before «#» and «::»
    int sprite = 0;
    NpcKind kind = NpcKind::Other;
};
struct Warp {
    int x = 0, y = 0;
    std::string dest;  // the map it leads to (another one)
    int dx = 0, dy = 0;
};
struct MobOnMap {
    int id = 0;
    int count = 0;      // the map's spawn lines of it summed
    int respawnMs = 0;  // the longest of them (after death, as the server counts it)
};
struct Mob {
    std::string name;
    int lv = 0;
    bool boss = false;  // mode & 0x20 (MVPs and the bosses of the site's MVP timers)
};
struct MapHints {
    std::vector<Npc> npcs;
    std::vector<Warp> warps;
    std::vector<MobOnMap> mobs;  // the bosses first, then by count, then by name
};
struct Index {
    long long data = 0;  // the pack's stamp
    std::string commit;
    std::unordered_map<std::string, MapHints> maps;
    std::unordered_map<int, Mob> mobs;
};

// gzip (RFC 1952): one member, its CRC-32 and size checked; false when it is not one
bool gunzip(const std::string& gz, std::string& out);
// The pack's JSON (format 1) into the index; false when it is not such a pack
bool build(const std::string& json, Index& out);
// The site's NPC kinds by the name (GameReference / Shapes::npcKind) and their colours (0xRRGGBB, the site's map)
NpcKind kindOf(const std::string& name);
uint32_t colorOf(NpcKind k);
// The client's data\mapnametable.txt («prt_fild07.rsw#Prontera Field #»): the map's code (lower case) -> its name
// (trimmed)
std::map<std::string, std::string> mapNames(const std::string& text);
// A name for the minimap's one line: up to its first comma («Prontera City, Capitol of Rune-Midgard» - «Prontera City»)
std::string shortName(const std::string& name);

}  // namespace uagfx::refindex
