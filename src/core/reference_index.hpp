#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace uagfx::refindex {

// 3.0.11-beta.6 (the owner: «1. давай» - the hints on the new minimap): the game's reference pack (core/reference_pack,
// format 1) read for the new interface - per map its NPCs (as the site's map shows them), its warps to other maps and
// its monsters with their counts; 3.0.11-beta.7 (the owner: «сделай по максимум») the rest for the «Довідник» window:
// the items, the monsters' cards, the shops, Kafra's teleports, the pets, the crafting, what refers to what, and the
// search over it all. The parts that need no Windows.

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
// 3.0.11-beta.7: a teleport of a Kafra (or a ferryman, an NPC's script) from a map: where it is and where to
struct Teleport {
    int x = 0, y = 0;
    std::string npc, dest;
    int dx = 0, dy = 0;
    int price = -1;  // zeny; -1 none known (free or changing)
    bool kafra = false, conditional = false;
};
struct MobOnMap {
    int id = 0;
    int count = 0;      // the map's spawn lines of it summed
    int respawnMs = 0;  // the longest of them (after death, as the server counts it)
};
struct Drop {
    int item = 0;
    int rate = 0;  // in 0.01 % as mob_db keeps it (7000 = 70 %)
    uint8_t kind = 0;  // 0 a drop, 1 the MVP's reward, 2 its card
};
struct Mob {
    std::string name;
    int lv = 0;
    bool boss = false;  // mode & 0x20 (MVPs and the bosses of the site's MVP timers)
    int hp = 0, baseExp = 0, jobExp = 0, size = 0, race = 0, element = 0, elementLv = 1;  // 3.0.11-beta.7
    std::vector<Drop> drops;
};
struct Item {  // 3.0.11-beta.7
    std::string name, aegis;
    int type = 0, buy = 0, weight = 0, slots = 0;
};
struct Shop {
    std::string map, name;
    int x = 0, y = 0;
    std::vector<std::pair<int, int>> items;  // item, price (as the shop sells it)
};
struct Pet {
    int mob = 0, lure = 0, egg = 0, equip = 0, food = 0;
};
struct Craft {
    int item = 0, itemLv = 0, skill = 0, skillLv = 0;
    std::vector<std::pair<int, int>> mats;  // item, how many
};
struct MapHints {
    std::vector<Npc> npcs;
    std::vector<Warp> warps;
    std::vector<MobOnMap> mobs;  // the bosses first, then by count, then by name
    std::vector<Teleport> teleports;  // 3.0.11-beta.7: Kafra's and the NPCs' from here
    std::vector<std::string> flags;   // 3.0.11-beta.7: town, pvp, noteleport, ...
};
struct Where {  // a monster's map
    std::string map;
    int count = 0, respawnMs = 0;
};
struct Source {  // who drops an item
    int mob = 0, rate = 0;
    uint8_t kind = 0;
};
struct Index {
    long long data = 0;  // the pack's stamp
    std::string commit, date;
    std::unordered_map<std::string, MapHints> maps;
    std::unordered_map<int, Mob> mobs;
    std::unordered_map<int, Item> items;  // 3.0.11-beta.7 and below
    std::vector<Shop> shops;
    std::vector<Pet> pets;
    std::vector<Craft> craft;
    std::map<std::string, std::string> mapNames;  // the client's (data\mapnametable.txt), put by whoever has the client
    // what refers to what, made once
    std::unordered_map<int, std::vector<Source>> droppedBy;            // item -> monsters, the highest chance first
    std::unordered_map<int, std::vector<std::pair<int, int>>> soldAt;  // item -> (shop, price), the cheapest first
    std::unordered_map<int, std::vector<Where>> spawnsOf;              // monster -> maps, the most first
    std::unordered_map<int, std::vector<int>> craftOf;                 // item -> craft rows making it
    std::unordered_map<int, std::vector<int>> usedIn;                  // item -> craft rows taking it
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

// 3.0.11-beta.7: the search of «Довідник» - every word of the query a part of the name (case folded, any order: the
// console's rule), monsters, items and maps (by the code and the client's name); the closest first in each kind:
// the whole name, then its start or a word's start, then anywhere; then items found in more places, monsters met on
// maps (the lower first), maps with monsters
enum class Kind : uint8_t { Mob, Item, Map };
struct Hit {
    Kind kind = Kind::Mob;
    int id = 0;        // a monster's or an item's
    std::string map;   // a map's code
};
struct Found {
    std::vector<Hit> mobs, items, maps;
    size_t mobsAll = 0, itemsAll = 0, mapsAll = 0;  // before `most` cut each list
};
Found search(const Index& ix, const std::string& query, size_t most);
// A map's name to show: the client's short one with the code («Prontera Field (prt_fild07)»), else the code
std::string mapTitle(const Index& ix, const std::string& code);

}  // namespace uagfx::refindex
