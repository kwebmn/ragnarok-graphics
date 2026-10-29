#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/reference_index.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::refpages {

// 3.0.11-beta.7 (the owner: «сделай по максимум»): the pages of the «Довідник» window (the concept's «Справочник» -
// a part of the interface): the first screen of sections, the search's groups, a section's list, the cards of a
// monster, an item, a map and a shop. A page is rows of words; each row may lead to another page. No Windows here:
// win/reference_window lays the rows out and draws them.

enum class Go : uint8_t { None, Mob, Item, Map, Shop, Section, Expand };  // Expand: a card's long section in full
struct Link {
    Go go = Go::None;
    int id = 0;       // a monster's, an item's, a shop's (its place in the index) or a section's
    std::string map;  // a map's code
};
enum class Style : uint8_t { Title, Sub, Heading, Row, Note, Tile };
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

Page home(const refindex::Index& ix, settings::Lang l);
Page found(const refindex::Index& ix, settings::Lang l, const std::string& query);
Page section(const refindex::Index& ix, settings::Lang l, int id);
// `here`: the player's map (it comes first in «Де знайти», marked; a map's card shows the way from it - 3.0.11-beta.8)
// `opened`: bits of Part shown in full; `hx`, `hy`: the player's cell (-1 not known)
Page mob(const refindex::Index& ix, settings::Lang l, int id, const std::string& here, unsigned opened = 0);
Page item(const refindex::Index& ix, settings::Lang l, int id, unsigned opened = 0);
Page map(const refindex::Index& ix, settings::Lang l, const std::string& code, const std::string& here,
         unsigned opened = 0, int hx = -1, int hy = -1);
Page shop(const refindex::Index& ix, settings::Lang l, int index);
Page open(const refindex::Index& ix, settings::Lang l, const Link& link, const std::string& here,
          unsigned opened = 0, int hx = -1, int hy = -1);  // by its kind

// Words for numbers: 1 200 (en 1,200); a chance kept in 0.01 % - «70 %», «1,5 %», «0,01 %» (en «1.5 %»)
std::string number(long long v, settings::Lang l);
std::string chance(int rate, settings::Lang l);
std::string elementName(int element, settings::Lang l);
std::string raceName(int race, settings::Lang l);
std::string sizeName(int size, settings::Lang l);
std::string typeName(int type, settings::Lang l);
// A respawn after death: «90 хв», from two hours «2 год», «11 год 30 хв»
std::string respawn(int ms, settings::Lang l);

}  // namespace uagfx::refpages
