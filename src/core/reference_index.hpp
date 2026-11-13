#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/reference_quests.hpp"
#include "core/walk_grid.hpp"

namespace uagfx::refindex {

// 3.0.11-beta.6 (the owner: «1. давай» - the hints on the new minimap): the game's reference pack (core/reference_pack,
// format 1) read for the new interface - per map its NPCs (as the site's map shows them), its warps to other maps and
// its monsters with their counts; 3.0.11-beta.7 (the owner: «сделай по максимум») the rest for the «Довідник» window:
// the items, the monsters' cards, the shops, Kafra's teleports, the pets, the crafting, what refers to what, and the
// search over it all. The parts that need no Windows.

// 3.0.18-beta.240 (the reference's maps, the owner 13.11): Quest, City, Custom (the server's own), Guild, Event - the
// kinds the site's pack tells by the NPC's script folder; the rest guessed by the name as before
enum class NpcKind : uint8_t { Kafra, Healer, Tool, Weapon, Armor, Inn, Guide, Job, Warp, Other, Quest, City, Custom,
                               Guild, Event };
// The groups the maps and the reference show: Kafra «K», the merchants «$» (the shops, the healers, the inns), the
// quests «!», ours and the guides by their colour, the rest grey
enum class NpcGroup : uint8_t { Kafra, Merchant, Quest, Custom, Guide, City, Guild, Job, Event, Ferry, Other };
NpcGroup groupOf(NpcKind k);
const char* letterOf(NpcKind k);  // "K", "$", "!" or ""

struct Npc {
    int x = 0, y = 0;
    std::string name;  // as the site cleans it: before «#» and «::»
    int sprite = 0;
    NpcKind kind = NpcKind::Other;
    std::string folded{};  // 3.0.11-beta.28: its name case folded once by build() (the search's; "" not yet)
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
// 3.0.11-beta.8: every way off a spot of a map for the routes (core/reference_route) - the warps, the NPCs' and
// Kafra's moves, and the ones within the map (a tower's stairs) too
enum class How : uint8_t { Warp, Npc, Kafra };
struct Move {
    int x = 0, y = 0;  // the warp's cell or the NPC's
    std::string to;    // the map it puts you on (this one for the stairs)
    int dx = 0, dy = 0;
    How how = How::Warp;
    std::string npc;  // the NPC's name (a warp's own: not shown)
    int price = -1;   // zeny; -1 free or not known
    bool paid = false;         // Kafra's; an NPC's with a price or one that varies
    bool conditional = false;  // the script checks something (a quest, an item)
    bool depends = false;      // where it leads depends on something: one free NPC to several maps - Eden's way
                               // back, the Clock Tower's random warps, Ant Hell's exits; never a route's move
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
    // 3.0.18-beta.239 (the owner's review of 13.11): a monster's several entries of one thing (and kind) are one drop -
    // `rate` their chance together, 1 - (1 - a)(1 - b)...; these the entries' rates (empty: one entry)
    std::vector<int> parts;
};
struct Mob {
    std::string name;
    int lv = 0;
    bool boss = false;  // mode & 0x20 (MVPs and the bosses of the site's MVP timers)
    int hp = 0, baseExp = 0, jobExp = 0, size = 0, race = 0, element = 0, elementLv = 1;  // 3.0.11-beta.7
    std::vector<Drop> drops;
    std::string sprite;  // 3.0.11-beta.14: the server's sprite name (mob_db), lower case; "" in older packs
    // 3.0.11-beta.28: its other name - kRO's where the game says iRO's («Wander Man» of Wanderer); "" none or the same
    std::string other;
    std::string folded{}, otherFolded{};  // 3.0.11-beta.28: both case folded once by build() ("" not yet)
    // 3.0.18-beta.253 (R-JSWF77 «мало информации про моба, дай полную инфу, статы»): the site's newer packs' stats
    // (its 16th field) - `has` false in the older ones
    struct Stats {
        bool has = false;
        int atkMin = 0, atkMax = 0, def = 0, mdef = 0, str = 0, agi = 0, vit = 0, intel = 0, dex = 0, luk = 0;
        int range = 0, speed = 0, attackDelay = 0, mode = 0;  // cells; ms a cell; ms between attacks; mob_db's mode
    } stats;
};
struct Item {  // 3.0.11-beta.7
    std::string name, aegis;
    int type = 0, buy = 0, weight = 0, slots = 0;
    // 3.0.18-beta.253: the site's newer packs' (its 8th field) - attack, defence, weapon level, equip level
    bool hasStats = false;
    int atk = 0, def = 0, weaponLv = 0, equipLv = 0;
    std::string folded{}, aegisFolded{};  // 3.0.11-beta.28: both case folded once by build() ("" not yet)
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
// 3.0.18-beta.240: a monster's spawn area on its map (the pack's x, y and half sizes; a whole map's line has none)
struct Zone {
    int mob = 0, count = 0, x = 0, y = 0, rx = 0, ry = 0;
};
struct MapHints {
    std::vector<Npc> npcs;
    std::vector<Zone> zones;  // 3.0.18-beta.240
    std::vector<Warp> warps;
    std::vector<MobOnMap> mobs;  // the bosses first, then by count, then by name
    std::vector<Teleport> teleports;  // 3.0.11-beta.7: Kafra's and the NPCs' from here
    std::vector<std::string> flags;   // 3.0.11-beta.7: town, pvp, noteleport, ...
    std::vector<Move> moves;          // 3.0.11-beta.8
};
struct Where {  // a monster's map
    std::string map;
    int count = 0, respawnMs = 0;
};
struct Source {  // who drops an item
    int mob = 0, rate = 0;
    uint8_t kind = 0;
    std::vector<int> parts;  // 3.0.18-beta.239: the drop's merged entries (Drop::parts)
};
}  // namespace uagfx::refindex
namespace uagfx::refroute {
struct Graph;
}
namespace uagfx::refindex {

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
    // 3.0.11-beta.16: the site's quests (core/reference_quests; read with the pack) and the items they take
    std::vector<refquests::Quest> quests;
    std::unordered_map<int, std::vector<int>> questsOf;  // item -> quests taking it
    // 3.0.11-beta.8: the maps' walkable parts (uaro-graphics\walkable.bin; none: routes by whole maps) and the
    // routes' graph made from them once (core/reference_route: prepare)
    std::shared_ptr<const walkgrid::Grids> walk;
    std::shared_ptr<const refroute::Graph> graph;
};

// gzip (RFC 1952): one member, its CRC-32 and size checked; false when it is not one
bool gunzip(const std::string& gz, std::string& out);
// The pack's JSON (format 1) into the index; false when it is not such a pack
bool build(const std::string& json, Index& out);
// 3.0.18-beta.250 (the owner: «убери ... итемы»): a thing one can get on the server - a monster on a map drops it, a
// shop sells it, a craft makes it, a quest or a pet takes it; the reference lists and finds only those
bool obtainable(const Index& ix, int item);
// The site's NPC kinds by the name (GameReference / Shapes::npcKind) and their colours (0xRRGGBB, the site's map)
NpcKind kindOf(const std::string& name);
// 3.0.18-beta.240: by the site's folder word (kafra, merchant, quest, city, custom, guild, job, guide, event, ferry,
// other); another word: the name's guess
NpcKind kindOfFolder(const std::string& folder, const std::string& name);
uint32_t colorOf(NpcKind k);
// The client's data\mapnametable.txt («prt_fild07.rsw#Prontera Field #»): the map's code (lower case) -> its name
// (trimmed)
std::map<std::string, std::string> mapNames(const std::string& text);
// A name for the minimap's one line: up to its first comma («Prontera City, Capitol of Rune-Midgard» - «Prontera City»)
std::string shortName(const std::string& name);
// 3.0.11-beta.200 (the tester: «в паті лісті виводиться назва файла мапи»): a map's words for a list - the client's
// name, its code's number added where the name is more than one map's («Payon Cave 3»), else its code
std::string mapWords(const std::map<std::string, std::string>& names, const std::string& code);
// 3.0.11-beta.14: the client's sprite files a monster may have, the likeliest first: the server's sprite name, it
// without an event's «_» or a variant's prefix (m_desert_wolf - desert_wolf: the client draws them alike), its name
std::vector<std::string> spriteNames(const Mob& m);

// 3.0.11-beta.7: the search of «Довідник» - every word of the query a part of the name (case folded, any order: the
// console's rule), monsters, items and maps (by the code and the client's name); the closest first in each kind:
// the whole name, then its start or a word's start, then anywhere; then items found in more places, monsters met on
// maps (the lower first), maps with monsters; 3.0.11-beta.15: the NPCs by their names, all of them (`most` is the
// other kinds')
// 3.0.11-beta.28 (the concept's v21_search): monsters by their other names too, the items by their service names
// (aegis) when their names have it not - a list of their own
enum class Kind : uint8_t { Mob, Item, Map, Npc };  // Npc (3.0.11-beta.15): `map`'s NPC number `id`
struct Hit {
    Kind kind = Kind::Mob;
    int id = 0;        // a monster's or an item's
    std::string map;   // a map's code
    uint8_t by = 0;    // 3.0.11-beta.28: found by its name 0, by a monster's other name 1, an item's service name 2
};
struct Found {
    std::vector<Hit> mobs, items, maps, npcs;
    std::vector<Hit> aegis;  // 3.0.11-beta.28: the items whose service names alone have it (Peco_Wing_Feather)
    size_t mobsAll = 0, itemsAll = 0, mapsAll = 0, npcsAll = 0, aegisAll = 0;  // before `most` cut each list
};
Found search(const Index& ix, const std::string& query, size_t most);
// A map's name to show: the client's short one with the code («Prontera Field (prt_fild07)»), else the code
std::string mapTitle(const Index& ix, const std::string& code);

}  // namespace uagfx::refindex
