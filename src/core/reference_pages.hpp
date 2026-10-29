#pragma once
#include <cstdint>
#include <map>
#include <string>
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
// Kafra (again: no more); World (3.0.11-beta.11): the world map - id 0 the world, 1 the dungeons
enum class Go : uint8_t { None, Mob, Item, Map, Shop, Section, Expand, Follow, World };
struct Link {
    Go go = Go::None;
    int id = 0;       // a monster's, an item's, a shop's (its place in the index) or a section's
    std::string map;  // a map's code
};
enum class Style : uint8_t { Title, Sub, Heading, Row, Note, Tile, World };  // World: the world map's picture
struct Row {
    Style style = Style::Row;
    std::string left, right;  // UTF-8: the row's words, and a note on its right (a tile: its count and its line)
    Link link;
    bool gold = false;   // a boss, the place the player is
    uint32_t dot = 0;    // 0xRRGGBB of a mark before the words (an NPC's kind), 0 none
};
struct Page {
    std::string name;  // what «Назад до …» calls it
    std::vector<Row> rows;
};

// Sections of the first screen (Go::Section's ids); items by their type are 100 + the type
enum Section : int { Mobs, Bosses, Items, Cards, Maps, Shops, Teleports, Pets, Crafting, kSections };
// A card's long sections show their first kFew rows and «усі N ›» (Go::Expand, its bit) until opened (the concept:
// the main things first - «Де знайти» and «Що випадає» - the rest below without a long scroll)
enum Part : int {
    Where, Drops, DroppedBy, SoldAt, WhoHere, Exits, Teleports2, Npcs, ShopsHere, UsedIn,
    Route1, Route2  // 3.0.11-beta.8: «Як дістатися» on foot, by Kafra
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
};

// `online`: the server's players now (3.0.11-beta.10; 0 not known)
Page home(const refindex::Index& ix, settings::Lang l, int online = 0);
Page found(const refindex::Index& ix, settings::Lang l, const std::string& query);
Page section(const refindex::Index& ix, settings::Lang l, int id);
// `here`: the player's map (it comes first in «Де знайти», marked; a map's card shows the way from it - 3.0.11-beta.8)
// `opened`: bits of Part shown in full
Page mob(const refindex::Index& ix, settings::Lang l, int id, const Context& c, unsigned opened = 0);
// «Де купити» (3.0.11-beta.12): of the cheapest the nearest to the player's map first, and how far
Page item(const refindex::Index& ix, settings::Lang l, int id, const Context& c, unsigned opened = 0);
Page map(const refindex::Index& ix, settings::Lang l, const std::string& code, const Context& c, unsigned opened = 0);
Page shop(const refindex::Index& ix, settings::Lang l, int index);
// 3.0.11-beta.11: the world map (its picture row, the tile under the pointer told under it) and the dungeons by groups
Page world(const refindex::Index& ix, settings::Lang l, const Context& c);
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
// 3.0.11-beta.8: where a route's move is taken - «вихід 156, 22», «Kafra 146, 86 · 1 200 z», «перехід 52, 136»
std::string stepNote(const refroute::Step& step, settings::Lang l);

}  // namespace uagfx::refpages
