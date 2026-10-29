#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/reference_index.hpp"
#include "core/reference_live.hpp"
#include "core/reference_route.hpp"
#include "core/world_map.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::refpages {

// 3.0.11-beta.7 (the owner: «сделай по максимум»): the pages of the «Довідник» window (the concept's «Справочник» -
// a part of the interface): the first screen of sections, the search's groups, a section's list, the cards of a
// monster, an item, a map and a shop. A page is rows of words; each row may lead to another page. No Windows here:
// win/reference_window lays the rows out and draws them.

// Expand: a card's long section in full; Follow (3.0.11-beta.9): the way to `map` on the minimap - id 0 on foot, 1 by
// Kafra (again: no more); World (3.0.11-beta.11): the world map - id 0 the world, 1 the dungeons; Quests, Quest
// (3.0.11-beta.16): the site's quests by kinds, one quest (its place in the index)
// Fold (3.0.11-beta.17): a card's section folded or unfolded by the player (id: its Part) - in every card alike;
// Star (3.0.11-beta.18): the card in «Обране» or out of it; Zoom (3.0.11-beta.29): the world map's zoom (the window's)
// - id -1 out, 0 the whole map, 1 in
enum class Go : uint8_t { None, Mob, Item, Map, Shop, Section, Expand, Follow, World, Quests, Quest, Fold, Star, Zoom };
struct Link {
    Go go = Go::None;
    int id = 0;       // a monster's, an item's, a shop's (its place in the index) or a section's
    std::string map;  // a map's code
};
// World: the world map's picture; Portrait (3.0.11-beta.14): a card's picture (the monster's first frame, the item's
// icon) with its two lines beside it - `left` and `right`, one under the other
// Text (3.0.11-beta.16): words that wrap - a quest's description, its steps (`left`)
// 3.0.11-beta.26, the concept's look: Search - the search's own big field (the first screen, the results; `right` its
// key); Hint - a line under it; Tile - `left` its name, `right` its count, `sub` what it holds, `icon` "ui:<name>";
// Block - a framed group of the rows after it (`left` its name, `right` a note at its right; `share` > 0: beside the
// next block, the line's share in per cent); Chip - a card in a block's chips (`left`, `sub` its kind, `picture`)
// 3.0.11-beta.28, the concept's search results (v21_search): Result - one found (`icon`, `left` its name, `sub` its
// line, `right` its kind, `marks` the query in them); Group - their kind's capitals and count (`right` a note at its
// right); More - «ще N ›» that opens the group; Empty - nothing found (`left` its line, `sub` what to try)
// 3.0.11-beta.29, the concept's world map (v19_route): Tab - its bar's tab (`icon`, `gold` the page's) or its chip (no
// link: `dot`); Tool - its zoom's button (`icon`); Caption - the panel's title; Warn - a warning's box (`left`
// wrapped); Step - a way's step (`right` its number, `left` where to, `sub` how, `tone` 1 paid); Legend - what a mark
// on the map means (`icon` "mark:route", "mark:jump", "mark:players", "mark:here"); Button - the panel's (`gold` the
// main one)
enum class Style : uint8_t {
    Title, Sub, Heading, Row, Note, Tile, World, Portrait, Text, Search, Hint, Block, Chip, Result, Group, More, Empty,
    Tab, Tool, Caption, Warn, Step, Legend, Button
};
using Marks = std::vector<std::pair<uint16_t, uint16_t>>;  // byte ranges [from, to) of the words
struct Row {
    Style style = Style::Row;
    std::string left, right;  // UTF-8: the row's words, and a note on its right (a tile: its count)
    Link link;
    bool gold = false;   // a boss, the place the player is
    uint32_t dot = 0;    // 0xRRGGBB of a mark before the words (an NPC's kind), 0 none
    std::string picture;  // 3.0.11-beta.14: a Portrait's - "mob:<id>", "item:<id>" (win/reference_window finds it)
    std::string icon;     // 3.0.11-beta.15: a small picture before the words - "item:<id>" (every row of an item)
    int fold = -1;        // 3.0.11-beta.17: a section's heading the player folds (its Part), -1 none
    std::string sub;      // 3.0.11-beta.26: a tile's line, a chip's kind
    // 3.0.11-beta.26: a block beside the next one - its share of the line, per cent (100: a block alone); a card's
    // section heading with a share is one of its main blocks (never folded), without one a section that folds
    int share = 0;
    bool closed = false;            // 3.0.11-beta.26: a folded section's heading (drawn ▸, the rows left out)
    int tone = 0;                   // 3.0.11-beta.26: a chip's number - 1 good (green), -1 heals (red)
    std::vector<std::string> tags;  // 3.0.11-beta.26: a card's picture row's tags («elN:» an element's, «gold:»)
    Marks marks, subMarks;          // 3.0.11-beta.28: a result's query in `left`, in `sub`
};
struct Page {
    std::string name;  // what «Назад до …» calls it
    std::vector<Row> rows;
    int star = -1;  // 3.0.11-beta.26: a card's star in the header - -1 none, 0 not starred, 1 in «Обране»
    // 3.0.11-beta.26: the concept's sections at the window's left (v23_ref_monster): «Усе», then each (Tile rows:
    // `left`, `right` its count, `icon`, `link`), the page's own one lit (-1 none)
    std::vector<Row> rail;
    int railOn = -1;
};

// Sections of the first screen (Go::Section's ids); items by their type are 100 + the type; 3.0.11-beta.26 the
// concept's «NPC і послуги» (the shops and Kafra) and «Таблиці» (the elements)
enum Section : int { Mobs, Bosses, Items, Cards, Maps, Shops, Teleports, Pets, Crafting, Services, Tables, kSections };
// A card's long sections show their first kFew rows and «усі N ›» (Go::Expand, its bit) until opened (the concept:
// the main things first - «Де знайти» and «Що випадає» - the rest below without a long scroll)
enum Part : int {
    Where, Drops, DroppedBy, SoldAt, WhoHere, Exits, Teleports2, Npcs, ShopsHere, UsedIn,
    Route1, Route2,  // 3.0.11-beta.8: «Як дістатися» on foot, by Kafra
    Elements,        // 3.0.11-beta.13: «Шкода стихіями»
    // 3.0.11-beta.17: the other sections a player may fold
    Way, Timers, Market, QuestFor, Kafra, PetOf, CraftOf, QuestNpc, QuestItems, QuestSteps, QuestReward
};
constexpr size_t kFew = 5;

// 3.0.11-beta.9: whether a way can be followed on the minimap (the new one on) and the one followed now
struct Follow {
    bool can = false;
    std::string map;   // where the followed way leads ("" none)
    bool walk = true;  // on foot (else by Kafra)
};
// Where the player is and what the window knows now - a map's card says more with it
struct Context {
    std::string here;    // the player's map
    int x = -1, y = -1;  // the player's cell (-1 not known): the routes start from that part of the map
    Follow follow;
    const std::map<std::string, int>* players = nullptr;  // 3.0.11-beta.10: players on each map now (the site's)
    const worldmap::Sheet* world = nullptr;  // 3.0.11-beta.11: the site's world map sheet (null: not here yet)
    std::string hover;                       // 3.0.11-beta.11: the world map's tile under the pointer
    const reflive::Live* live = nullptr;     // 3.0.11-beta.12: the players' public MVP timers, the site's market
    long long now = 0;                       // unix time: the timers' minutes
    unsigned folded = 0;                     // 3.0.11-beta.17: the sections (Part bits) the player folded
    // 3.0.11-beta.18: the cards opened last (the latest first) and the ones the player starred - the first screen's;
    // a card says whether it is starred
    const std::vector<Link>* recent = nullptr;
    const std::vector<Link>* favourites = nullptr;
};
// 3.0.11-beta.18: a card's name for the first screen's lists («Poring», «Fly Wing», «Prontera Field (prt_fild08)»);
// "" when the index has no such
std::string titleOf(const refindex::Index& ix, settings::Lang l, const Link& link);
bool sameCard(const Link& a, const Link& b);
std::string linkText(const Link& link);           // «m 1002», «p prt_fild08» - a line of the kept lists
bool linkOf(const std::string& line, Link& out);  // back; false: not one

// `c` (3.0.11-beta.18): its recent and starred; 3.0.11-beta.26 the concept's: the search, ten sections, the recent
// and the starred as chips (the players now: the window's bottom)
Page home(const refindex::Index& ix, settings::Lang l, const Context& c = {});
// `c` (3.0.11-beta.15): the NPCs found the nearest to the player's map first, and how far
// 3.0.11-beta.28 the concept's (v21_search): each kind a group of its first 8 and «ще N ›» (Go::Expand: `opened`'s
// bits), the query marked in the names; the items found by their service names apart; nothing - what to try
Page found(const refindex::Index& ix, settings::Lang l, const std::string& query, const Context& c = {},
           unsigned opened = 0);
Page section(const refindex::Index& ix, settings::Lang l, int id, const Context& c = {});
// 3.0.11-beta.26: a page with the concept's sections at its left, the one `at` belongs to lit (a monster's card the
// monsters', an item's the items', ...; the search's «Усе»)
Page withRail(Page p, const refindex::Index& ix, settings::Lang l, const Link& at);
// `here`: the player's map (it comes first in «Де знайти», marked; a map's card shows the way from it - 3.0.11-beta.8)
// `opened`: bits of Part shown in full
Page mob(const refindex::Index& ix, settings::Lang l, int id, const Context& c, unsigned opened = 0);
// «Де купити» (3.0.11-beta.12): of the cheapest the nearest to the player's map first, and how far
Page item(const refindex::Index& ix, settings::Lang l, int id, const Context& c, unsigned opened = 0);
Page map(const refindex::Index& ix, settings::Lang l, const std::string& code, const Context& c, unsigned opened = 0);
Page shop(const refindex::Index& ix, settings::Lang l, int index);
// 3.0.11-beta.16: the site's quests by kinds, a quest's card (who gives it, what it takes, its steps, its reward)
Page quests(const refindex::Index& ix, settings::Lang l);
Page quest(const refindex::Index& ix, settings::Lang l, int index);
// 3.0.11-beta.11: the world map (its picture row) and the dungeons by groups; `marks` (3.0.11-beta.19): "mob:<id>" -
// the monster's maps marked on it with its numbers there, its dungeons apart; "item:<id>" (3.0.11-beta.20) - the
// maps of the monsters dropping it, how many of them; 3.0.11-beta.29 the concept's v19_route: its bar (the tabs, the
// players now, the zoom), the map, a panel at its right - "to:<code>" (":w" on foot, ":k" paying) the way there
Page world(const refindex::Index& ix, settings::Lang l, const Context& c, const std::string& marks = "");
// 3.0.11-beta.29: a way's move on the world map - from a map to the next; `jump` a teleport (Kafra's, an NPC's - a
// dashed arc; `label` its words)
struct Hop {
    std::string from, to;
    bool jump = false;
    std::string label;
};
// A world map's target in its marks ("to:<code>"; `variant` -1 the better way, 0 on foot, 1 paying)
bool worldTarget(const std::string& marks, std::string& code, int& variant);
// The way chosen there from the player's place (`variant` -1: the better - set to the one taken)
refroute::Route worldRoute(const refindex::Index& ix, const Context& c, const std::string& code, int& variant);
std::vector<Hop> worldHops(const refindex::Index& ix, settings::Lang l, const refroute::Route& r);
// A map's card on the world map under the pointer: its name, its code and monsters' kinds ("" none to say), the players
// there now (none: no third line)
std::vector<std::string> worldTip(const refindex::Index& ix, settings::Lang l, const std::string& map,
                                  const Context& c);
// The towns' names written on their tiles (code, name)
std::vector<std::pair<std::string, std::string>> worldLabels(const refindex::Index& ix);
// What the world map shows of a page's way (its target's, else the way followed on the minimap): its moves, its end,
// the end's pin's words (a dungeon's «… → підземелля»), the player's words («Ви тут · …»)
struct WorldWay {
    std::vector<Hop> hops;
    std::string target, targetLabel, hereLabel;
};
WorldWay worldWayOf(const refindex::Index& ix, settings::Lang l, const Context& c, const std::string& marks);
// The part of the sheet a way and the maps marked take (false: none on the sheet) - the view zoomed on it
bool worldFocus(const worldmap::Sheet& sheet, const WorldWay& way, const std::map<std::string, int>& marks, float& l,
                float& t, float& r, float& b);
// The maps a world map's `marks` names, with the numbers to show on them
std::map<std::string, int> worldMarks(const refindex::Index& ix, const std::string& marks);
Page dungeons(const refindex::Index& ix, settings::Lang l, const Context& c);
Page open(const refindex::Index& ix, settings::Lang l, const Link& link, const Context& c,
          unsigned opened = 0);  // by its kind

// Words for numbers: 1 200 (en 1,200); a chance kept in 0.01 % - «70 %», «1,5 %», «0,01 %» (en «1.5 %»)
std::string number(long long v, settings::Lang l);
std::string chance(int rate, settings::Lang l);
std::string elementName(int element, settings::Lang l);
std::string raceName(int race, settings::Lang l);
std::string sizeName(int size, settings::Lang l);
std::string typeName(int type, settings::Lang l);
// A respawn after death: «90 хв», from two hours «2 год», «11 год 30 хв»
std::string respawn(int ms, settings::Lang l);
// 3.0.11-beta.13: the server's element table (db/attr_fix.txt, src/core/attr_fix.inc) - the per cent of an attack of
// `attack`'s element a monster of `defence`'s element at `level` (1-4) takes; below 0 it heals; 100 when out of range
int elementDamage(int attack, int defence, int level);
uint32_t elementColour(int element);  // 3.0.11-beta.26: the concept's dot of an element
// 3.0.11-beta.8: where a route's move is taken - «вихід 156, 22», «Kafra 146, 86 · 1 200 z», «перехід 52, 136»
std::string stepNote(const refroute::Step& step, settings::Lang l);

}  // namespace uagfx::refpages
