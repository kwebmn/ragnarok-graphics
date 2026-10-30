// 3.0.11-beta.7: «Довідник» - its pages from the reference pack (core/reference_pages) and its window's shapes
// (core/reference_window): what a card says and where its rows lead, the words for numbers, the layout and the clicks.
#include <cmath>
#include <string>

#include "core/reference_live.hpp"
#include "core/bar_hint.hpp"
#include "core/reference_pages.hpp"
#include "core/reference_route.hpp"
#include "core/reference_window.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

namespace {

refpages::Context ctx(const std::string& here, const refpages::Follow& follow = {}) {
    refpages::Context c;
    c.here = here;
    c.follow = follow;
    return c;
}

// A pack cut down: Poring (here on prt_fild08 and on prt_fild05), a boss, items with drops, two shops, crafting, a pet
const char* kPack = R"({"v":1,"data":5,"commit":"d68ea17ea5b8","date":"2026-10-23T19:20:17+03:00",
"items":{"909":["Jellopy","Jellopy",3,6,1,0,0],"512":["Apple","Apple",0,15,2,0,0],
         "601":["Fly Wing","Wing_Of_Fly",2,60,5,0,0],
         "4001":["Poring Card","Poring_Card",6,20,10,0,0],"1101":["Sword","Sword",4,100,500,3,2],
         "998":["Iron","Iron",3,50,50,0,0],"531":["Apple Juice","Apple_Juice",0,20,4,0,0]},
"mobs":{"1002":["Poring",1,50,2,1,1,3,1,1,0,[[909,7000],[512,1000],[601,150]],[],[4001,1]],
        "1917":["Satan Morroc",99,60000000,0,0,2,6,7,4,1,[[601,9000]],[[909,5000]],null]},
"spawns":{"prt_fild08":[[1002,100,0,0,0,0,0]],"prt_fild05":[[1002,70,0,0,0,0,0]],
          "moc_fild20":[[1917,1,200,200,0,0,41400000]]},
"routes":[["prt_fild08",170,378,"prontera",156,22,"w","",null,0],["prt_fild08",171,378,"prontera",156,22,"w","",null,0],
          ["prontera",151,29,"izlude",91,105,"k","Kafra",600,0],["prontera",152,29,"izlude",91,105,"k","Kafra",600,0]],
"shops":[["prontera",134,203,"Tool Dealer",[[601,60],[512,15]]],["izlude",110,150,"Tool Dealer",[[601,55]]]],
"npcs":[["prontera",150,180,"Kafra Employee",113]],
"pets":[[1002,512,9001,10013,531]],
"craft":[[1101,1,94,1,[[998,1]]]],
"flags":{"prontera":["town","nomemo"]}})";

refindex::Index index() {
    refindex::Index ix;
    CHECK(refindex::build(kPack, ix));
    ix.mapNames = refindex::mapNames(
        "prt_fild08.rsw#Prontera Field #\nprontera.rsw#Prontera City, Capitol of Rune-Midgard#\n"
        "izlude.rsw#Izlude Town#");
    return ix;
}

const refpages::Row* rowWith(const refpages::Page& p, const std::string& left) {
    for (const refpages::Row& r : p.rows)
        if (r.left == left) return &r;
    return nullptr;
}

size_t indexOf(const refpages::Page& p, const std::string& left) {
    for (size_t i = 0; i < p.rows.size(); ++i)
        if (p.rows[i].left == left) return i;
    return size_t(-1);
}

}  // namespace

TEST(reference_pages_words_for_numbers) {
    CHECK_EQ(refpages::chance(7000, Lang::Uk), std::string("70 %"));
    CHECK_EQ(refpages::chance(150, Lang::Uk), std::string("1,5 %"));
    CHECK_EQ(refpages::chance(1, Lang::Uk), std::string("0,01 %"));
    CHECK_EQ(refpages::chance(150, Lang::En), std::string("1.5 %"));
    CHECK_EQ(refpages::number(1200, Lang::Uk), std::string("1 200"));
    CHECK_EQ(refpages::number(60000000, Lang::En), std::string("60,000,000"));
    CHECK_EQ(refpages::number(999, Lang::Uk), std::string("999"));
    CHECK_EQ(refpages::respawn(600000, Lang::Uk), std::string("10 хв"));
    CHECK_EQ(refpages::respawn(7200000, Lang::Uk), std::string("2 год"));
    CHECK_EQ(refpages::respawn(41400000, Lang::Uk), std::string("11 год 30 хв"));
    CHECK_EQ(refpages::elementName(1, Lang::Uk), std::string("Water"));  // 3.0.11-beta.30: the game's names
    CHECK_EQ(refpages::raceName(3, Lang::En), std::string("Plant"));
    CHECK_EQ(refpages::typeName(6, Lang::Uk), std::string("Картка"));
}

TEST(reference_pages_home_and_search) {
    const refindex::Index ix = index();
    // 3.0.11-beta.26: the concept's first screen - its search and its line, ten tiles, «Обране» (empty: a word)
    const refpages::Page home = refpages::home(ix, Lang::Uk);
    CHECK_EQ(home.rows.size(), size_t(14));
    CHECK(home.rows[0].style == refpages::Style::Search && home.rows[0].right == "Ctrl+F");
    CHECK(home.rows[1].style == refpages::Style::Hint);
    CHECK(home.rows[2].style == refpages::Style::Tile && home.rows[2].left == "Монстри");
    CHECK(home.rows[2].right == "2" && home.rows[2].icon == "ui:monster");
    CHECK_EQ(home.rows[2].sub, std::string("рівень, стихія, де водяться, що падає"));
    CHECK(home.rows[2].link.go == refpages::Go::Section && home.rows[2].link.id == refpages::Mobs);
    const char* names[] = {"Монстри", "Предмети", "Локації",     "Квести",
                           "NPC і послуги", "MVP", "Пети", "Крафт",
                           "Карта світу", "Таблиці"};  // the concept's order
    for (size_t i = 0; i < 10; ++i) CHECK_EQ(home.rows[2 + i].left, std::string(names[i]));
    CHECK(home.rows[10].link.go == refpages::Go::World && home.rows[10].right.empty());
    CHECK(home.rows[12].style == refpages::Style::Block && home.rows[12].left == "Обране");
    CHECK_EQ(home.rows[12].share, 0);
    CHECK(home.rows[13].style == refpages::Style::Note);
    const refpages::Page found = refpages::found(ix, Lang::Uk, "por");
    CHECK(found.rows[0].style == refpages::Style::Search && found.rows[0].right == "Esc");
    CHECK(found.rows[1].style == refpages::Style::Group);  // 3.0.11-beta.28: the concept's groups of results
    const refpages::Row* poring = rowWith(found, "Poring");
    CHECK(poring && poring->link.go == refpages::Go::Mob && poring->link.id == 1002);
    CHECK(rowWith(found, "Poring Card") != nullptr);  // the items too
    CHECK(rowWith(refpages::found(ix, Lang::Uk, "prontera city"), "Prontera City (prontera)") != nullptr);  // a map
    const refpages::Page none = refpages::found(ix, Lang::Uk, "nothing like it");
    CHECK(none.rows.size() == 2 && none.rows[1].style == refpages::Style::Empty);
    CHECK_EQ(none.rows[1].left, std::string("За «nothing like it» нічого не знайдено"));
}

TEST(reference_pages_a_monsters_card) {
    const refindex::Index ix = index();
    const refpages::Page p = refpages::mob(ix, Lang::Uk, 1002, ctx("prt_fild05"));
    CHECK(p.rows[0].style == refpages::Style::Title && p.rows[0].left == "Poring" && p.rows[0].right.empty());
    // 3.0.11-beta.26: the concept's head - its tags (the level, the element), its line
    CHECK(p.rows[1].tags.size() == 2 && p.rows[1].tags[0] == "Lv 1" && p.rows[1].tags[1] == "el1:Water 1");
    CHECK_EQ(p.rows[1].left, std::string("Plant · Medium · HP 50 · досвід 2 / 1"));  // 3.0.11-beta.30: the game's
    CHECK(p.rows[1].style == refpages::Style::Portrait && p.rows[1].picture == "mob:1002");  // 3.0.11-beta.14
    // «Де знайти»: the player's map first (marked), then by count
    const size_t where = indexOf(p, "Де знайти");
    CHECK(p.rows[where + 1].link.map == "prt_fild05" && p.rows[where + 1].gold);
    CHECK(p.rows[where + 1].right.find("ви тут") == 0);
    CHECK_EQ(p.rows[where + 2].left, std::string("Prontera Field (prt_fild08)"));
    // «Що випадає»: the drops by chance, then the card; each to its item
    const size_t drops = indexOf(p, "Що випадає");
    CHECK(p.rows[drops + 1].left == "Jellopy" && p.rows[drops + 1].right == "70 %" && p.rows[drops + 1].link.id == 909);
    CHECK(p.rows[drops + 3].left == "Fly Wing" && p.rows[drops + 3].right == "1,5 %");
    CHECK(p.rows[drops + 4].left == "Poring Card" && p.rows[drops + 4].right == "0,01 %");
    const refpages::Row* lure = rowWith(p, "Apple");  // the pet's taming item (the drop row comes first)
    CHECK(lure != nullptr);
    CHECK(indexOf(p, "Улюбленець") != size_t(-1));
    const refpages::Page boss = refpages::mob(ix, Lang::Uk, 1917, ctx(""));
    CHECK(boss.rows[0].gold);
    CHECK(rowWith(boss, "Morroc Field (moc_fild20)") == nullptr);  // no client name known: the code
    const refpages::Row* there = rowWith(boss, "moc_fild20");
    CHECK(there && there->right == "×1 · 11 год 30 хв");
    const refpages::Row* reward = rowWith(boss, "Jellopy");
    CHECK(reward && reward->right == "нагорода MVP · 50 %");
}

TEST(reference_pages_an_items_card) {
    const refindex::Index ix = index();
    const refpages::Page p = refpages::item(ix, Lang::Uk, 601, ctx(""));
    CHECK(p.rows[0].left == "Fly Wing" && p.rows[0].right.empty());
    CHECK(p.rows[1].left == "вага 0,5 · ціна 60 z · продаж 30 z");  // 3.0.11-beta.22: an NPC pays half
    CHECK(p.rows[1].picture == "item:601" && p.rows[1].tags.size() == 2);  // 3.0.11-beta.26: the concept's tags
    CHECK(p.rows[1].tags[0] == "Вживане" && p.rows[1].tags[1] == "Wing_Of_Fly");
    const size_t by = indexOf(p, "З кого випадає");
    CHECK_EQ(p.rows[by].right, std::string("2"));
    CHECK(p.rows[by + 1].left == "Satan Morroc" && p.rows[by + 1].gold);  // 90 % before 1,5 %
    CHECK(p.rows[by + 2].left == "Poring");
    const size_t at = indexOf(p, "Де купити");
    CHECK(p.rows[at + 1].left == "Tool Dealer · Izlude Town (izlude)" && p.rows[at + 1].right == "55 z");  // cheaper
    CHECK(p.rows[at + 1].link.go == refpages::Go::Shop);
    const refpages::Page sword = refpages::item(ix, Lang::Uk, 1101, ctx(""));
    CHECK(sword.rows[0].left == "Sword [3]");
    const size_t made = indexOf(sword, "З чого зробити");
    CHECK(sword.rows[made + 1].left == "Iron" && sword.rows[made + 1].right == "×1");
    const refpages::Page iron = refpages::item(ix, Lang::Uk, 998, ctx(""));
    CHECK(rowWith(iron, "Sword") != nullptr);  // «Входить у крафт»
    const refpages::Page juice = refpages::item(ix, Lang::Uk, 531, ctx(""));
    const refpages::Row* pet = rowWith(juice, "Poring");
    CHECK(pet && pet->right == "корм" && pet->link.go == refpages::Go::Mob);
}

TEST(reference_pages_a_maps_card) {
    const refindex::Index ix = index();
    const refpages::Page p = refpages::map(ix, Lang::Uk, "prontera", ctx("prontera"));
    CHECK(p.rows[0].left == "Prontera City" && p.rows[0].gold);
    // 3.0.11-beta.26: the concept's head - its minimap, its code and its marks as tags, its full name
    CHECK(p.rows[1].style == refpages::Style::Portrait && p.rows[1].picture == "map:prontera");
    CHECK_EQ(p.rows[1].left, std::string("Prontera City, Capitol of Rune-Midgard"));
    CHECK(p.rows[1].tags.size() == 4 && p.rows[1].tags[0] == "prontera" && p.rows[1].tags[1] == "місто");
    CHECK(p.rows[1].tags[3] == "gold:ви тут");
    const size_t tp = indexOf(p, "Телепорти");
    CHECK_EQ(p.rows[tp].right, std::string("1"));  // two Kafra rows to the same place, once
    CHECK(p.rows[tp + 1].left == "Kafra → Izlude Town (izlude)" && p.rows[tp + 1].right == "600 z");
    CHECK(p.rows[tp + 1].link.map == "izlude");
    CHECK(rowWith(p, "Tool Dealer") != nullptr);
    const size_t near = indexOf(p, "Kafra поруч");  // 3.0.11-beta.15: on this very map
    CHECK(near != size_t(-1) && p.rows[near + 1].left == "Kafra Employee");
    CHECK_EQ(p.rows[near + 1].right, std::string("тут · 150, 180"));
    const refpages::Row& kafra = p.rows[p.rows.size() - 1];  // the NPCs' list, the last
    CHECK(kafra.left == "Kafra Employee" && kafra.dot == 0xD6336C && kafra.right == "150, 180");
    const refpages::Page izlude = refpages::map(ix, Lang::Uk, "izlude", ctx(""));  // none there: by the teleport
    const size_t far = indexOf(izlude, "Kafra поруч");
    CHECK(far == size_t(-1));  // the way back is not known to the pack: no Kafra found
    const refpages::Page field = refpages::map(ix, Lang::Uk, "prt_fild08", ctx(""));
    const size_t exits = indexOf(field, "Виходи");
    CHECK_EQ(field.rows[exits].right, std::string("1"));  // two warps to Prontera, one exit
    CHECK(field.rows[exits + 1].link.map == "prontera");
}

TEST(reference_pages_long_sections_open_by_their_row) {
    // 3.0.11-beta.30 (the owner: «пускай будет весь список, разворачивать имеет смысл если там больше 20 позиций»):
    // a monster on 30 maps - the first 20 and «усі 30 ›», which opens the section in place; on 25 - all of them
    const auto onMaps = [](int maps) {
        std::string spawns;
        for (int i = 0; i < maps; ++i)
            spawns += std::string(i ? "," : "") + "\"m" + std::to_string(i) + "\":[[1,8,0,0,0,0,0]]";
        return spawns;
    };
    refindex::Index all;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{"1":["A",1,1,0,0,0,0,0,1,0,[],[],null]},"spawns":{)" +
                              onMaps(25) + R"(},"routes":[],"npcs":[]})",
                          all));
    CHECK(rowWith(refpages::mob(all, Lang::Uk, 1, ctx("m6")), "усі 25 ›") == nullptr);
    const std::string spawns = onMaps(30);
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{"1":["A",1,1,0,0,0,0,0,1,0,[],[],null]},"spawns":{)" + spawns +
                              R"(},"routes":[],"npcs":[]})",
                          ix));
    const refpages::Page closed = refpages::mob(ix, Lang::Uk, 1, ctx("m6"));
    const size_t where = indexOf(closed, "Де знайти");
    CHECK_EQ(closed.rows[where].right, std::string("30"));
    CHECK(closed.rows[where + 1].link.map == "m6");  // the player's map first, before the cut
    const refpages::Row& more = closed.rows[where + 1 + refpages::kFew];
    CHECK(more.left == "усі 30 ›" && more.link.go == refpages::Go::Expand && more.link.id == refpages::Where);
    const refpages::Page open = refpages::mob(ix, Lang::Uk, 1, ctx("m6"), 1u << refpages::Where);
    CHECK_EQ(open.rows.size(), closed.rows.size() + 30 - refpages::kFew - 1);
    CHECK(rowWith(open, "усі 30 ›") == nullptr);
}

TEST(reference_window_lays_itself_out) {
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    CHECK(L.w == 1270 && L.h == 850 && L.x == 325 && L.y == 115);  // the concept's size, centred
    const refwin::Layout small = refwin::layout(1366, 768, 1.0f);
    CHECK(small.w == 1270 && small.h == 728);  // 20 px of the game left round it
    const refwin::Layout big = refwin::layout(3840, 2160, 2.0f);
    CHECK(big.w == 2540 && big.h == 1700);
    const refindex::Index ix = index();
    const refpages::Page home = refpages::home(ix, Lang::Uk);
    int total = 0;
    const std::vector<refwin::Placed> rows = refwin::place(L, home, total);
    // 3.0.11-beta.26: the concept's - the search, its line, five tiles to a line, «Обране» under them
    CHECK(rows[0].y < rows[1].y && rows[1].y < rows[2].y && rows[0].h == 54);
    CHECK(rows[2].y == rows[6].y && rows[7].y > rows[2].y && rows[3].x > rows[2].x && rows[7].x == rows[2].x);
    CHECK(rows[12].y > rows[7].y + rows[7].h && rows[12].w == rows[0].w && rows[13].y > rows[12].y);
    CHECK(total > rows[12].y + rows[12].h);
    refwin::Texts t;
    t.title = "Довідник";
    const refwin::Chrome first = refwin::chrome(L, home, t);
    CHECK(first.back.w == 0 && first.search.w == 0 && first.star.w == 0 && first.close.w > 0);  // its own field
    // clicks: ×, the field, a tile; nothing outside
    CHECK(refwin::under(L, home, rows, 0, first, float(L.w - 20), 30).part == refwin::Part::Close);
    CHECK(refwin::under(L, home, rows, 0, first, 200, float(L.bodyY + rows[0].y + 20)).part == refwin::Part::Search);
    const refwin::Under tile = refwin::under(L, home, rows, 0, first, float(rows[4].x + 20),
                                             float(L.bodyY + rows[4].y + 20));
    CHECK(tile.part == refwin::Part::Row && tile.row == 4);
    CHECK(refwin::under(L, home, rows, 0, first, -5, 10).part == refwin::Part::None);
    CHECK(refwin::under(L, home, rows, 0, first, 130, 30).part != refwin::Part::Back);  // none on the first
    t.back = "Назад до Довідник";
    t.crumbs = {"Монстри", "Poring"};
    const refpages::Page poring = refpages::mob(ix, Lang::Uk, 1002, ctx("prontera"));
    const refwin::Chrome card = refwin::chrome(L, poring, t);
    CHECK(card.back.w > 0 && card.search.w > 0 && card.crumbs.size() == 3);  // two parts and a «›»
    CHECK(card.back.x > first.title.x + first.title.w && card.search.x > card.crumbs.back().x);
    CHECK(refwin::under(L, poring, rows, 0, card, card.back.x + 10, 30).part == refwin::Part::Back);
    CHECK(refwin::under(L, poring, rows, 0, card, card.search.x + 30, 30).part == refwin::Part::Search);
    CHECK_EQ(refwin::clampScroll(-40, total, L.bodyH), 0);
    CHECK_EQ(refwin::clampScroll(100000, 2000, 500), 1500);
}

TEST(reference_window_paints_and_words_its_rows) {
    const refindex::Index ix = index();
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    const refpages::Page p = refpages::mob(ix, Lang::Uk, 1002, ctx("prt_fild08"));
    int total = 0;
    const std::vector<refwin::Placed> rows = refwin::place(L, p, total);
    panel::Picture pic;
    refwin::paint(pic, L, p, rows, 0, total, refwin::Look{});
    CHECK(pic.width == L.w && pic.height == L.h && pic.px.size() == size_t(L.w) * size_t(L.h));
    refwin::Texts t;
    t.title = "Довідник";
    t.find = "Пошук у довіднику";
    t.pack = "Пакет даних";
    refwin::Look look;
    look.chrome = refwin::chrome(L, p, t);
    const std::vector<refwin::Words> words = refwin::words(L, p, rows, 0, t, look);
    bool title = false, typed = false, poring = false;
    for (const refwin::Words& w : words) {
        title = title || w.text == "Довідник";
        typed = typed || w.text == "Пошук у довіднику";  // 3.0.11-beta.26: the header's search on a card
        poring = poring || (w.text == "Poring" && w.px == 26.0f);
        CHECK(w.box.x >= 0 && w.box.y >= 0 && w.box.x + w.box.w <= float(L.w) + 0.5f);
        CHECK(w.box.y + w.box.h <= float(L.h));
    }
    CHECK(title && typed && poring);
}

// 3.0.11-beta.25 (R-77TCM7:
// «когда по меню вожу, лагат курсор и фпс падает»): another row under the pointer
// repaints only the old highlights' areas and the new ones' - the picture as painted whole
TEST(reference_window_hover_repaint_matches_the_whole) {
    const refindex::Index ix = index();
    int steps = 0, differing = 0, outside = 0;
    for (const float k : {1.0f, 1.5f, 2.0f}) {
        const refwin::Layout L = refwin::layout(int(1920 * k), int(1080 * k), k);
        const refpages::Page pages[] = {refpages::home(ix, Lang::Uk),
                                        refpages::mob(ix, Lang::Uk, 1002, ctx("prontera")),
                                        refpages::found(ix, Lang::Uk, "po")};
        for (const refpages::Page& p : pages) {
            int total = 0;
            const std::vector<refwin::Placed> rows = refwin::place(L, p, total);
            std::vector<int> links;
            for (size_t i = 0; i < p.rows.size(); ++i)
                if (p.rows[i].link.go != refpages::Go::None && refwin::shown(L, rows[i], 0)) links.push_back(int(i));
            if (links.size() < 2) continue;
            const refwin::Under seq[] = {{},
                                         {refwin::Part::Row, links[0]},
                                         {refwin::Part::Row, links[1]},
                                         {refwin::Part::Close, -1},
                                         {refwin::Part::Back, -1},
                                         {refwin::Part::Row, links.back()},
                                         {}};
            refwin::Texts texts;
            texts.title = "Довідник";
            texts.back = "Назад до Довідник";
            refwin::Look was;
            was.chrome = refwin::chrome(L, p, texts);
            was.selected = links.back();
            panel::Picture partial;
            refwin::paint(partial, L, p, rows, 0, total, was);
            for (const refwin::Under& u : seq) {
                refwin::Look now = was;
                now.hover = u;
                for (const refwin::Look* l : {&was, &now})
                    for (const Rect& a : refwin::hotAreas(L, p, rows, 0, *l)) {
                        const std::vector<uint32_t> before = partial.px;
                        refwin::repaint(partial, L, p, rows, 0, total, now, a);
                        for (int y = 0; y < L.h; y += 3)  // nothing out of the area: only it goes to the card
                            for (int x = 0; x < L.w; ++x) {
                                const size_t i = size_t(y) * size_t(L.w) + size_t(x);
                                const bool in = x >= a.left && x < a.right && y >= a.top && y < a.bottom;
                                if (!in && partial.px[i] != before[i]) ++outside;
                            }
                    }
                panel::Picture whole;
                refwin::paint(whole, L, p, rows, 0, total, now);
                ++steps;
                if (partial.px != whole.px) ++differing;
                was = now;
            }
        }
    }
    CHECK(steps >= 14);
    CHECK_EQ(differing, 0);
    CHECK_EQ(outside, 0);
}

TEST(reference_window_hot_areas) {
    const refindex::Index ix = index();
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    const refpages::Page p = refpages::home(ix, Lang::Uk);
    int total = 0;
    const std::vector<refwin::Placed> rows = refwin::place(L, p, total);
    refwin::Look look;
    CHECK(refwin::hotAreas(L, p, rows, 0, look).empty());
    look.hover = refwin::Under{refwin::Part::Row, 0};
    const std::vector<Rect> one = refwin::hotAreas(L, p, rows, 0, look);
    CHECK_EQ(one.size(), size_t(1));
    CHECK(!one.empty() && one[0].left <= rows[0].x && one[0].right >= rows[0].x + rows[0].w);
    CHECK(!one.empty() && one[0].top <= L.bodyY + rows[0].y && one[0].bottom >= L.bodyY + rows[0].y + rows[0].h);
    CHECK(!one.empty() && one[0].right <= L.w && one[0].bottom <= L.h && one[0].left >= 0);
    look.hover = refwin::Under{refwin::Part::Back, -1};
    CHECK(refwin::hotAreas(L, p, rows, 0, look).empty());  // no «back» on the first page
    refwin::Texts back;
    back.back = "Назад до Довідник";
    look.chrome = refwin::chrome(L, p, back);
    look.selected = 1;
    CHECK_EQ(refwin::hotAreas(L, p, rows, 0, look).size(), size_t(2));
}

// 3.0.11-beta.12: the site's live data (core/reference_live) - the players' public MVP timers, the market - in the
// cards
TEST(reference_live_timers_and_market) {
    const char* site = R"({"v":1,"at":5,"mvp":[[1917,"moc_fild20",1000],[1917,"moc_fild20",400],[1002,"",900],
        [0,"x",1],[1917,"moc fild\"20",1]],"market":[[601,55,3,40],[909,0,1,1],[512,15,0,0]]})";
    reflive::Live live;
    CHECK(reflive::fromSite(site, 2000, live));
    CHECK_EQ(live.at, 2000LL);
    CHECK_EQ(live.timers.size(), size_t(4));        // a timer of no monster left out
    CHECK_EQ(live.timers.front().killed, 1000LL);   // the latest first
    CHECK_EQ(live.timers.back().map, std::string("mocfild20"));  // a map's code: nothing else kept
    CHECK_EQ(live.market.size(), size_t(1));        // no price, no lots: left out
    CHECK_EQ(live.market.at(601).cheapest, 55LL);
    reflive::Live back;
    CHECK(reflive::read(reflive::write(live), back));
    CHECK_EQ(back.timers.size(), size_t(4));
    CHECK_EQ(back.market.at(601).pieces, 40);
    CHECK(reflive::timerOf(back, 1917, "moc_fild20")->killed == 1000);
    CHECK(reflive::timerOf(back, 1002, "prt_fild08") != nullptr);  // a timer without a map: any map
    CHECK(reflive::timerOf(back, 1003) == nullptr);
    CHECK(!reflive::fromSite("{\"v\":1,\"mvp\":[]}", 1, live));

    const refindex::Index ix = index();
    refpages::Context c = ctx("");
    c.live = &back;
    c.now = 1000 + 30 * 60;  // half an hour after the kill; Satan Morroc's respawn there: 11 h 30 min
    const refpages::Page boss = refpages::mob(ix, Lang::Uk, 1917, c);
    const size_t at = indexOf(boss, "Таймери гравців");
    CHECK(at != size_t(-1));
    CHECK_EQ(boss.rows[at + 1].right, std::string("через 11 год · вбито 30 хв тому"));
    CHECK(!boss.rows[at + 1].gold);
    c.now = 1000 + 12 * 3600;
    const refpages::Page due = refpages::mob(ix, Lang::Uk, 1917, c);
    CHECK_EQ(due.rows[at + 1].right, std::string("вже має бути · вбито 12 год тому"));
    CHECK(due.rows[at + 1].gold);
    const refpages::Page wing = refpages::item(ix, Lang::Uk, 601, c);
    const size_t m = indexOf(wing, "На ринку сайту");
    CHECK(m != size_t(-1));
    CHECK_EQ(wing.rows[m + 1].left, std::string("від 55 z"));
    CHECK_EQ(wing.rows[m + 1].right, std::string("лотів: 3 · штук: 40"));
    CHECK(indexOf(refpages::item(ix, Lang::Uk, 909, c), "На ринку сайту") == size_t(-1));
    CHECK(indexOf(refpages::mob(ix, Lang::Uk, 1917, ctx("")), "Таймери гравців") == size_t(-1));  // none read
}

// 3.0.11-beta.13: the server's element table (db/attr_fix.txt) in a monster's card
TEST(reference_pages_damage_by_element) {
    CHECK_EQ(refpages::elementDamage(4, 1, 1), 175);  // wind on water 1
    CHECK_EQ(refpages::elementDamage(3, 1, 1), 50);   // fire on water 1
    CHECK_EQ(refpages::elementDamage(6, 6, 3), -50);  // holy on holy 3: it heals
    CHECK_EQ(refpages::elementDamage(0, 8, 1), 25);   // neutral on a ghost
    CHECK_EQ(refpages::elementDamage(10, 1, 1), 100);
    CHECK_EQ(refpages::elementDamage(1, 1, 5), 100);
    const refindex::Index ix = index();
    const refpages::Page p = refpages::mob(ix, Lang::Uk, 1002, ctx(""));  // Poring: water 1
    const size_t at = indexOf(p, "Шкода стихіями");
    CHECK(at != size_t(-1));
    CHECK_EQ(p.rows[at].right, std::string("Water 1 · Wind 175 %"));  // 3.0.11-beta.26: says the most
    CHECK(p.rows[at + 1].style == refpages::Style::Chip && p.rows[at + 1].left == "Wind");  // the most first
    CHECK_EQ(p.rows[at + 1].right, std::string("175 %"));
    CHECK(p.rows[at + 1].tone == 1 && p.rows[at + 1].dot == refpages::elementColour(4));
    CHECK_EQ(p.rows[at + 10].right, std::string("25 %"));  // all ten as chips: water on water, the least
    CHECK_EQ(p.rows[at + 10].tone, 0);
    CHECK(at + 11 >= p.rows.size() || p.rows[at + 11].left.rfind("Ваша панель", 0) != 0);  // no bar known: none
    // 3.0.11-beta.68 (the concept's «Ваша панель: Fire Bolt ×2…»): the hotbar's skills on it, after the chips
    refpages::Context c = ctx("");
    const std::vector<barhint::Skill> bar{{"MG_FIREBOLT", "Fire Bolt"}, {"AL_BLESSING", "Blessing"},
                                          {"WZ_JUPITEL", "Jupitel Thunder"}};
    c.bar = &bar;
    const refpages::Page withBar = refpages::mob(ix, Lang::Uk, 1002, c);
    const size_t yours = indexOf(withBar, "Шкода стихіями") + 11;
    CHECK(yours < withBar.rows.size());
    if (yours >= withBar.rows.size()) return;
    CHECK_EQ(withBar.rows[yours].left, std::string("Ваша панель (множник стихії): Jupitel Thunder \xC3\x97" "1,75 "
                                                   "\xC2\xB7 Fire Bolt \xC3\x97" "0,5"));
    std::vector<barhint::Skill> none{{"AL_BLESSING", "Blessing"}};  // nothing of an element of its own: no line
    c.bar = &none;
    const refpages::Page plain = refpages::mob(ix, Lang::Uk, 1002, c);
    CHECK_EQ(plain.rows.size(), p.rows.size());
}

// 3.0.11-beta.15: the NPCs found by their names (the nearest first), a map's nearest Kafra, the items' icons
TEST(reference_pages_npcs_kafra_and_icons) {
    refindex::Index ix = index();
    ix.graph = refroute::prepare(ix);
    const refpages::Page found = refpages::found(ix, Lang::Uk, "kafra", ctx("prt_fild08"));
    const size_t npcs = indexOf(found, "NPC · 1");
    CHECK(npcs != size_t(-1) && found.rows[npcs].right == "ближчі — вище");
    CHECK_EQ(found.rows[npcs + 1].left, std::string("Kafra Employee"));
    CHECK_EQ(found.rows[npcs + 1].sub, std::string("Prontera City (prontera) · 150, 180 · переходів: 1"));
    CHECK(found.rows[npcs + 1].right == "NPC" && found.rows[npcs + 1].icon == "ui:npc");
    CHECK(found.rows[npcs + 1].link.go == refpages::Go::Map && found.rows[npcs + 1].link.map == "prontera");
    const refpages::Page field = refpages::map(ix, Lang::Uk, "prt_fild08", ctx(""));
    const size_t near = indexOf(field, "Kafra поруч");
    CHECK(near != size_t(-1));
    CHECK_EQ(field.rows[near + 1].left, std::string("Prontera City (prontera)"));
    CHECK_EQ(field.rows[near + 1].right, std::string("переходів: 1 · 150, 180"));
    const refpages::Page poring = refpages::mob(ix, Lang::Uk, 1002, ctx(""));
    const size_t drops = indexOf(poring, "Що випадає");
    CHECK_EQ(poring.rows[drops + 1].icon, std::string("item:909"));  // Jellopy's icon before it
    CHECK(poring.rows[1].icon.empty());
}

// 3.0.11-beta.17: the sections the player folds - in every card alike (the heading alone, «›»)
TEST(reference_pages_sections_fold_by_choice) {
    const refindex::Index ix = index();
    refpages::Context c = ctx("");
    const refpages::Link poring{refpages::Go::Mob, 1002, ""};
    const refpages::Page open = refpages::open(ix, Lang::Uk, poring, c);
    const size_t drops = indexOf(open, "Що випадає");  // 3.0.11-beta.26: a main block - it never folds
    CHECK(open.rows[drops].link.go == refpages::Go::None && open.rows[drops].share == 50);
    const size_t ele = indexOf(open, "Шкода стихіями");
    CHECK(open.rows[ele].link.go == refpages::Go::Fold && open.rows[ele].link.id == refpages::Elements);
    CHECK(open.rows[ele + 1].left == "Wind" && !open.rows[ele].closed);
    c.folded = 1u << refpages::Elements | 1u << refpages::Drops;
    const refpages::Page shut = refpages::open(ix, Lang::Uk, poring, c);
    const size_t at = indexOf(shut, "Шкода стихіями");
    CHECK(shut.rows[at].closed && shut.rows[at].right == open.rows[ele].right);  // drawn ▸, its line kept
    CHECK(at + 1 == shut.rows.size() || shut.rows[at + 1].style == refpages::Style::Heading);  // its chips gone
    CHECK(indexOf(shut, "Jellopy") != size_t(-1));  // a main block's rows stay
    CHECK(indexOf(shut, "Де знайти") != size_t(-1));
}

// 3.0.11-beta.18: «Нещодавні» and «Обране» on the first screen, a card's star
TEST(reference_pages_recent_and_starred) {
    const refindex::Index ix = index();
    refpages::Link k;
    CHECK(refpages::linkOf("m 1002", k) && k.go == refpages::Go::Mob && k.id == 1002);
    CHECK(refpages::linkOf("p prt_fild08", k) && k.go == refpages::Go::Map && k.map == "prt_fild08");
    CHECK(!refpages::linkOf("p prt fild08", k));
    CHECK(!refpages::linkOf("x 1", k));
    CHECK_EQ(refpages::linkText(refpages::Link{refpages::Go::Item, 601, ""}), std::string("i 601"));
    const std::vector<refpages::Link> recent{{refpages::Go::Mob, 1002, ""}, {refpages::Go::Map, 0, "prt_fild08"},
                                             {refpages::Go::Mob, 9999, ""}};  // one the index has not: left out
    const std::vector<refpages::Link> starred{{refpages::Go::Item, 601, ""}};
    refpages::Context c = ctx("");
    c.recent = &recent;
    c.favourites = &starred;
    const refpages::Page home = refpages::home(ix, Lang::Uk, c);
    // 3.0.11-beta.26: the concept's two blocks side by side - the recent (1.4) and the starred (1), their chips
    const size_t fav = indexOf(home, "Обране"), lately = indexOf(home, "Нещодавні");
    CHECK(fav != size_t(-1) && lately != size_t(-1) && lately < fav);
    CHECK(home.rows[lately].style == refpages::Style::Block && home.rows[lately].share == 58);
    CHECK(home.rows[fav].style == refpages::Style::Block && home.rows[fav].share == 42);
    const refpages::Row& p0 = home.rows[lately + 1];
    CHECK(p0.style == refpages::Style::Chip && p0.left == "Poring");
    CHECK(p0.sub == "монстр" && p0.icon == "mob:1002");
    CHECK(home.rows[lately + 2].left == "Prontera Field" && home.rows[lately + 2].icon == "ui:location");  // no code
    CHECK_EQ(lately + 3, fav);  // the one the index has not: left out
    CHECK(home.rows[fav + 1].left == "Fly Wing" && home.rows[fav + 1].icon == "item:601");
    CHECK(home.rows[fav + 1].sub == "предмет" && home.rows[fav + 1].link.id == 601);
    int total = 0;
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    const std::vector<refwin::Placed> rows = refwin::place(L, home, total);
    CHECK(rows[lately].y == rows[fav].y && rows[fav].x > rows[lately].x + rows[lately].w);  // side by side
    CHECK(rows[lately].h == rows[fav].h && rows[lately].w > rows[fav].w);
    CHECK(rows[lately + 2].x > rows[lately + 1].x && rows[lately + 2].y == rows[lately + 1].y);  // chips in a line
    CHECK(rows[lately + 1].x >= rows[lately].x && rows[fav + 1].x >= rows[fav].x);
    // a card's star: in the header (the page's), not a row
    const refpages::Page wing = refpages::open(ix, Lang::Uk, refpages::Link{refpages::Go::Item, 601, ""}, c);
    CHECK_EQ(wing.star, 1);  // starred
    const refpages::Page poring = refpages::open(ix, Lang::Uk, refpages::Link{refpages::Go::Mob, 1002, ""}, c);
    CHECK_EQ(poring.star, 0);
    CHECK(rowWith(poring, "Додати в обране ›") == nullptr);
    CHECK_EQ(refpages::home(ix, Lang::Uk).star, -1);
}

// 3.0.11-beta.22: a number finds the item and the monster of that id
TEST(reference_pages_found_by_number) {
    const refindex::Index ix = index();
    const refpages::Page wing = refpages::found(ix, Lang::Uk, "601");  // 3.0.11-beta.26: under the search's field
    CHECK(wing.rows[1].left == "Номер 601" && wing.rows[2].left == "Fly Wing" && wing.rows[2].link.id == 601);
    const refpages::Page poring = refpages::found(ix, Lang::Uk, "1002");
    CHECK(poring.rows[2].left == "Poring" && poring.rows[2].link.go == refpages::Go::Mob);
    CHECK(indexOf(refpages::found(ix, Lang::Uk, "7"), "Номер 7") == size_t(-1));  // no such id: nothing for it
}

// 3.0.11-beta.26 (the owner:
// «у нас оно отличается от того как это выглядит в концепте»):
// the concept's sections, its blocks' capitals, the header's way that keeps its last parts when it does not fit
TEST(reference_concept_sections_and_words) {
    const refindex::Index ix = index();
    const refpages::Page services = refpages::section(ix, Lang::Uk, refpages::Services);
    CHECK_EQ(services.name, std::string("NPC і послуги"));
    CHECK(indexOf(services, "Телепорти Kafra") != size_t(-1));
    CHECK(indexOf(services, "Крамниці") != size_t(-1));
    const refpages::Page tables = refpages::section(ix, Lang::Uk, refpages::Tables);
    CHECK_EQ(tables.name, std::string("Таблиці"));
    CHECK(indexOf(tables, "Стихії атаки проти захисту рівня 1") != size_t(-1));
    CHECK(indexOf(tables, "Стихії атаки проти захисту рівня 4") != size_t(-1));
    const refpages::Row* fire = rowWith(tables, "Fire");  // 3.0.11-beta.32: a grid - fire against earth 150, water 50
    CHECK(fire && fire->style == refpages::Style::Grid && fire->tags.size() == 10);
    CHECK(fire && fire->tags[2] == "150" && fire->tags[1] == "50" && fire->tags[0] == "100");
    CHECK(tables.rows[1].style == refpages::Style::Grid && tables.rows[1].gold && tables.rows[1].tags[3] == "Fire");
    CHECK(tables.rows[1].tags[0] == "Neutral");  // the columns' whole names: the window cuts them only when narrow
    const auto columnHead = [&](const refwin::Layout& L) {
        int total = 0;
        const std::vector<refwin::Placed> placed = refwin::place(L, tables, total);
        refwin::Texts none;
        refwin::Look plain;
        plain.chrome = refwin::chrome(L, tables, none);
        for (const refwin::Words& w : refwin::words(L, tables, placed, 0, none, plain))
            if (w.bold && w.text.rfind("Neu", 0) == 0) return w;
        return refwin::Words{};
    };
    CHECK_EQ(columnHead(refwin::layout(1920, 1080, 1.0f)).text, std::string("Neutral"));
    const refwin::Layout slim = refwin::layout(800, 600, 1.0f);
    const refwin::Words cut = columnHead(slim);
    CHECK(cut.text.size() >= 3 && cut.text.size() < 7 && slim.measure(cut.text, cut.px, true) <= cut.box.w);
    // the blocks' names in capitals, the first screen's words
    const std::vector<refpages::Link> recent{{refpages::Go::Mob, 1002, ""}};
    refpages::Context c = ctx("");
    c.recent = &recent;
    const refpages::Page home = refpages::home(ix, Lang::Uk, c);
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    int total = 0;
    const std::vector<refwin::Placed> rows = refwin::place(L, home, total);
    refwin::Texts t;
    t.title = "Довідник";
    t.hint = "Пошук: монстр, предмет, карта, квест, NPC";
    refwin::Look look;
    look.chrome = refwin::chrome(L, home, t);
    bool lately = false, starred = false, hint = false, key = false;
    for (const refwin::Words& w : refwin::words(L, home, rows, 0, t, look)) {
        lately = lately || w.text == "НЕЩОДАВНІ";
        starred = starred || w.text == "ОБРАНЕ";
        hint = hint || (w.text == t.hint && w.px == 17.0f);
        key = key || w.text == "Ctrl+F";
    }
    CHECK(lately && starred && hint && key);
    // a narrow window keeps the way's last parts
    const refwin::Layout narrow = refwin::layout(1024, 768, 1.0f);
    const refpages::Page poring = refpages::mob(ix, Lang::Uk, 1002, ctx("prontera"));
    t.back = "Назад до Monster Section With A Long Name";
    t.crumbs = {"Монстри з дуже довгою назвою", "Ще одна довга сторінка",
                "Prontera Field", "Poring"};
    const refwin::Chrome c1 = refwin::chrome(narrow, poring, t);
    CHECK(c1.crumbFirst > 0 && !c1.crumbs.empty());
    CHECK(c1.crumbs.back().x + c1.crumbs.back().w <= c1.search.x);
    const refwin::Chrome c2 = refwin::chrome(L, poring, t);
    CHECK(c2.crumbFirst <= c1.crumbFirst);
}

// 3.0.11-beta.26, stage 2: the concept's cards (v23_ref_monster) - the rail of sections at the left, the card's head,
// its two main blocks side by side, the rest in sections that fold
TEST(reference_concept_cards) {
    const refindex::Index ix = index();
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    const refpages::Link poringAt{refpages::Go::Mob, 1002, ""};
    refpages::Page p = refpages::withRail(refpages::mob(ix, Lang::Uk, 1002, ctx("prontera")), ix, Lang::Uk, poringAt);
    CHECK_EQ(p.rail.size(), size_t(11));  // 3.0.11-beta.30: «Головна» and all the first screen's sections
    CHECK(p.rail[0].left == "Головна" && p.rail[0].icon == "ui:home" && p.rail[0].link.go == refpages::Go::None);
    CHECK(p.rail[9].left == "Карта світу" && p.rail[9].link.go == refpages::Go::World);
    CHECK(p.railOn == 1 && p.rail[1].left == "Монстри");
    const auto litFor = [&](const refpages::Link& k) {
        return refpages::withRail(refpages::Page{}, ix, Lang::Uk, k).railOn;
    };
    CHECK_EQ(litFor(refpages::Link{refpages::Go::Item, 601, ""}), 2);
    CHECK_EQ(litFor(refpages::Link{refpages::Go::Map, 0, "prontera"}), 3);
    CHECK_EQ(litFor(refpages::Link{refpages::Go::Quest, 0, ""}), 4);
    CHECK_EQ(litFor(refpages::Link{refpages::Go::Shop, 0, ""}), 5);
    CHECK_EQ(litFor(refpages::Link{refpages::Go::Section, refpages::Bosses, ""}), 6);
    CHECK_EQ(litFor(refpages::Link{refpages::Go::Section, 106, ""}), 2);  // the cards: an items' type
    CHECK_EQ(litFor(refpages::Link{}), 0);  // the search's results
    int total = 0;
    const std::vector<refwin::Placed> rows = refwin::place(L, p, total);
    refwin::Texts t;
    t.title = "Довідник";
    const refwin::Chrome c = refwin::chrome(L, p, t);
    CHECK(c.rail.size() == 11 && c.railW == 204.0f && !c.narrow);
    for (const refwin::Placed& r : rows) CHECK(r.x >= int(c.railW));  // the body right of the rail
    CHECK(refwin::under(L, p, rows, 0, c, c.rail[3].x + 20, c.rail[3].y + 10).part == refwin::Part::Rail);
    CHECK_EQ(refwin::under(L, p, rows, 0, c, c.rail[3].x + 20, c.rail[3].y + 10).row, 3);
    // the head: the picture at its left, the name beside it
    CHECK(rows[1].h == 112 && rows[0].x == rows[1].x + 112 + 18);
    // the main blocks side by side, the same height; the elements' section under them, its chips inside it
    const size_t where = indexOf(p, "Де знайти"), drops = indexOf(p, "Що випадає");
    const size_t ele = indexOf(p, "Шкода стихіями");
    CHECK(rows[where].y == rows[drops].y && rows[drops].x > rows[where].x + rows[where].w);
    CHECK(rows[where].boxH == rows[drops].boxH && rows[where].boxH > rows[where].h);
    CHECK(rows[ele].y >= rows[where].y + rows[where].boxH && rows[ele].h == 46 && rows[ele].boxH > rows[ele].h);
    CHECK(rows[ele + 1].in && rows[ele + 1].y > rows[ele].y && rows[ele + 2].x > rows[ele + 1].x);  // chips in a line
    CHECK(rows[where + 1].in && rows[where + 1].x > rows[where].x);
    // a folded section: its line alone
    refpages::Context folded = ctx("prontera");
    folded.folded = 1u << refpages::Elements;
    const refpages::Page shut =
        refpages::withRail(refpages::open(ix, Lang::Uk, poringAt, folded), ix, Lang::Uk, poringAt);
    const std::vector<refwin::Placed> shutRows = refwin::place(L, shut, total);
    const size_t at = indexOf(shut, "Шкода стихіями");
    CHECK(shut.rows[at].closed && shutRows[at].boxH == shutRows[at].h);
    // the words: ▸ for the folded, the block's capitals, the head's tags, the rail's names
    refwin::Look look;
    look.chrome = refwin::chrome(L, shut, t);
    bool arrow = false, capital = false, tag = false, rail = false;
    for (const refwin::Words& w : refwin::words(L, shut, shutRows, 0, t, look)) {
        arrow = arrow || w.text == "▸";
        capital = capital || w.text == "ДЕ ЗНАЙТИ";
        tag = tag || w.text == "Water 1";
        rail = rail || w.text == "Предмети";
    }
    CHECK(arrow && capital && tag && rail);
    // a narrow window: the rail's icons alone
    const refwin::Layout narrow = refwin::layout(1100, 700, 1.0f);
    const refwin::Chrome thin = refwin::chrome(narrow, shut, t);
    CHECK(thin.narrow && thin.railW == 58.0f && thin.rail.size() == 11);
    bool named = false;
    refwin::Look slim;
    slim.chrome = thin;
    for (const refwin::Words& w : refwin::words(narrow, shut, refwin::place(narrow, shut, total), 0, t, slim))
        named = named || w.text == "Предмети";
    CHECK(!named);
    // 3.0.11-beta.67 (the concept's small screens: the rail folded to its icons, «с подсказками»): the section under
    // the pointer names itself in a tip right of its icon, nothing else's words under it
    slim.hover = refwin::Under{refwin::Part::Rail, 2};
    int tips = 0;
    panel::Box tip;
    const std::vector<refwin::Words> slimWords =
        refwin::words(narrow, shut, refwin::place(narrow, shut, total), 0, t, slim);
    for (const refwin::Words& w : slimWords)
        if (w.text == "Предмети") {
            ++tips;
            tip = w.box;
        }
    CHECK_EQ(tips, 1);
    CHECK(tip.x >= thin.railW && std::fabs((tip.y + tip.h / 2) - (thin.rail[2].y + thin.rail[2].h / 2)) < 2);
    for (const refwin::Words& w : slimWords)
        if (w.text != "Предмети")
            CHECK(w.box.x >= tip.x + tip.w || w.box.x + w.box.w <= tip.x || w.box.y >= tip.y + tip.h ||
                  w.box.y + w.box.h <= tip.y);
}

TEST(reference_concept_rail_hover_repaints_its_section) {
    const refindex::Index ix = index();
    const refpages::Link at{refpages::Go::Item, 601, ""};
    const refpages::Page p = refpages::withRail(refpages::item(ix, Lang::Uk, 601, ctx("")), ix, Lang::Uk, at);
    for (const float k : {1.0f, 1.5f, 0.0f}) {  // 3.0.11-beta.67: 0 - a narrow window, its sections' tips
        const refwin::Layout L = k > 0 ? refwin::layout(int(1920 * k), int(1080 * k), k) : refwin::layout(1100, 700, 1);
        int total = 0;
        const std::vector<refwin::Placed> rows = refwin::place(L, p, total);
        refwin::Texts t;
        t.title = "Довідник";
        t.back = "Назад до Довідник";
        refwin::Look was;
        was.chrome = refwin::chrome(L, p, t);
        panel::Picture partial;
        refwin::paint(partial, L, p, rows, 0, total, was);
        for (const int i : {3, 7, 0, 5}) {
            refwin::Look now = was;
            now.hover = refwin::Under{refwin::Part::Rail, i};
            for (const refwin::Look* l : {&was, &now})
                for (const Rect& a : refwin::hotAreas(L, p, rows, 0, *l))
                    refwin::repaint(partial, L, p, rows, 0, total, now, a);
            panel::Picture whole;
            refwin::paint(whole, L, p, rows, 0, total, now);
            CHECK(partial.px == whole.px);
            was = now;
        }
    }
}

// 3.0.11-beta.28: the concept's search results (v21_search) - groups of results with their lines and kinds, the query
// marked, «ще N ›» that opens a group, the items by their service names apart, the monsters by their other names
TEST(reference_search_concept_results) {
    const refindex::Index ix = index();
    const refpages::Page wing = refpages::found(ix, Lang::Uk, "wing");
    CHECK(wing.rows[1].style == refpages::Style::Group && wing.rows[1].left == "Предмети · 1");
    CHECK_EQ(wing.rows[1].right, std::string("частіші — вище"));
    const refpages::Row& fly = wing.rows[2];
    CHECK(fly.style == refpages::Style::Result && fly.left == "Fly Wing" && fly.right == "предмет");
    CHECK_EQ(fly.sub, std::string("55 z · 2 крамниці · падає з 2 монстрів"));  // the cheaper shop's price
    CHECK(fly.icon == "item:601" && fly.link.go == refpages::Go::Item && fly.link.id == 601);
    CHECK((fly.marks == refpages::Marks{{4, 8}} && fly.subMarks.empty()));
    CHECK(wing.rows.back().style == refpages::Style::Hint);  // how the search finds, at the end
    // any order, any case; two words over one place - one mark
    CHECK((refpages::found(ix, Lang::Uk, "WING fly").rows[2].marks == (refpages::Marks{{0, 3}, {4, 8}})));
    CHECK((refpages::found(ix, Lang::Uk, "win wing").rows[2].marks == refpages::Marks{{4, 8}}));
    // the service name's group: its name has it not («Wing_Of_Fly»), marked in its line
    const refpages::Page of = refpages::found(ix, Lang::Uk, "of");
    const size_t aegis = indexOf(of, "За службовою назвою · 1");
    CHECK(aegis != size_t(-1) && of.rows[aegis + 1].left == "Fly Wing");
    CHECK_EQ(of.rows[aegis + 1].sub, std::string("службова назва Wing_Of_Fly"));
    CHECK((of.rows[aegis + 1].marks.empty() && of.rows[aegis + 1].subMarks == refpages::Marks{{33, 35}}));
    // a monster: MVP, its level, its maps; a boss's name gold
    const refpages::Page morroc = refpages::found(ix, Lang::Uk, "morroc");
    const refpages::Row* satan = rowWith(morroc, "Satan Morroc");
    CHECK(satan && satan->sub == "MVP · Lv 99 · 1 карта" && satan->gold && satan->icon == "mob:1917");
    const refpages::Page porings = refpages::found(ix, Lang::Uk, "poring");
    const refpages::Row* poring = rowWith(porings, "Poring");
    CHECK(poring && poring->sub == "Lv 1 · 2 карти" && poring->right == "монстр");
    // a location: a town, its monsters' kinds
    const refpages::Page towns = refpages::found(ix, Lang::Uk, "prontera");
    const refpages::Row* city = rowWith(towns, "Prontera City (prontera)");
    CHECK(city && city->sub == "місто" && city->icon == "ui:location" && city->right == "локація");
    const refpages::Page fields = refpages::found(ix, Lang::Uk, "field", ctx("prt_fild08"));
    const refpages::Row* field = rowWith(fields, "Prontera Field (prt_fild08)");
    CHECK(field && field->sub == "ви тут · 1 вид монстрів" && field->gold);
    // nothing: what was not found and what to try
    const refpages::Page none = refpages::found(ix, Lang::En, "zzz");
    CHECK(none.rows.size() == 2 && none.rows[1].style == refpages::Style::Empty);
    CHECK_EQ(none.rows[1].left, std::string("Nothing found for «zzz»"));
    CHECK(none.rows[1].sub.find("«wing»") != std::string::npos);
}

TEST(reference_search_concept_other_names) {
    std::string pack = kPack;
    const std::string card = "[4001,1]],\n";
    pack.replace(pack.find(card), card.size(), "[4001,1],\"poring\",\"Pink Jelly\"],\n");
    refindex::Index ix;
    CHECK(refindex::build(pack, ix));
    CHECK_EQ(ix.mobs.at(1002).other, std::string("Pink Jelly"));
    CHECK(ix.mobs.at(1917).other.empty());  // an older pack's monster
    const refindex::Found f = refindex::search(ix, "pink", 10);
    CHECK(f.mobs.size() == 1 && f.mobs[0].id == 1002 && f.mobs[0].by == 1);
    const refpages::Page p = refpages::found(ix, Lang::Uk, "pink");
    const refpages::Row& r = p.rows[2];
    CHECK(r.left == "Poring" && r.marks.empty());
    CHECK_EQ(r.sub, std::string("друга назва: Pink Jelly · Lv 1 · 2 карти"));
    CHECK((r.subMarks == refpages::Marks{{23, 27}}));  // «друга назва: » is 23 bytes
    // the same name in other letters is no other name
    std::string same = kPack;
    same.replace(same.find(card), card.size(), "[4001,1],\"poring\",\"PORING\"],\n");
    refindex::Index ix2;
    CHECK(refindex::build(same, ix2));
    CHECK(ix2.mobs.at(1002).other.empty());
}

TEST(reference_search_concept_more_and_forms) {
    refindex::Index ix;
    for (int i = 0; i < 30; ++i) {  // thirty stones; the first four sold in 1, 3, 5 and 21 shops
        const std::string name = "Stone " + std::string(1, char('A' + i / 10)) + std::string(1, char('a' + i % 10));
        ix.items[100 + i] = refindex::Item{name, "Stone", 3, 10, 1, 0};
        const int shops[] = {1, 3, 5, 21};
        if (i < 4)
            for (int s = 0; s < shops[i]; ++s) ix.soldAt[100 + i].emplace_back(s, 10);
    }
    const refpages::Page p = refpages::found(ix, Lang::Uk, "stone");
    CHECK(p.rows[1].style == refpages::Style::Group && p.rows[1].left == "Предмети · 30");
    CHECK(p.rows[2].left == "Stone Ad" && p.rows[2].sub == "10 z · 21 крамниця");  // the most shops first
    CHECK_EQ(p.rows[3].sub, std::string("10 z · 5 крамниць"));
    CHECK_EQ(p.rows[4].sub, std::string("10 z · 3 крамниці"));
    CHECK_EQ(p.rows[5].sub, std::string("10 z · 1 крамниця"));
    CHECK(p.rows[6].sub.empty());  // «Інше» unsaid, nothing else to say
    // 3.0.11-beta.30 (the owner: «разворачивать имеет смысл если там больше 20 позиций»): 20, «ще 10 ›»
    CHECK(p.rows[22].style == refpages::Style::More && p.rows[22].left == "ще 10 ›");
    CHECK(p.rows[22].link.go == refpages::Go::Expand);
    const refpages::Page open = refpages::found(ix, Lang::Uk, "stone", {}, 1u << unsigned(p.rows[22].link.id));
    CHECK(open.rows[31].style == refpages::Style::Result && open.rows[32].style == refpages::Style::Hint);
    CHECK_EQ(refpages::found(ix, Lang::En, "stone aa").rows[2].sub, std::string("10 z · 1 shop"));
    for (int i = 125; i < 130; ++i) ix.items.erase(i);  // 25: all of them, no «ще 5 ›»
    const refpages::Page most = refpages::found(ix, Lang::Uk, "stone");
    CHECK(most.rows[26].style == refpages::Style::Result && most.rows[27].style == refpages::Style::Hint);
}

TEST(reference_search_concept_window) {
    const refindex::Index ix = index();
    const refpages::Page p = refpages::withRail(refpages::found(ix, Lang::Uk, "wing"), ix, Lang::Uk, refpages::Link{});
    for (const float k : {1.0f, 2.0f}) {
        const refwin::Layout L = refwin::layout(int(1920 * k), int(1080 * k), k);
        int total = 0;
        const std::vector<refwin::Placed> rows = refwin::place(L, p, total);
        CHECK_EQ(rows[2].h, int(std::lround(50 * k)));  // a result's line
        CHECK_EQ(rows[1].h, int(std::lround(28 * k)));  // its group's
        const size_t last = p.rows.size() - 1;
        CHECK(rows[last].y >= rows[2].y + rows[2].h && rows[last].h >= int(std::lround(33 * k)));
        refwin::Texts t;
        t.title = "Довідник";
        t.query = "wing";
        refwin::Look look;
        look.chrome = refwin::chrome(L, p, t);
        look.selected = 2;
        const std::vector<refwin::Words> words = refwin::words(L, p, rows, 0, t, look);
        bool name = false, sub = false, kind = false, group = false;
        for (const refwin::Words& w : words) {
            name |= w.text == "Fly Wing" && w.bold && std::abs(w.px - 14.5f * k) < 0.01f;
            sub |= w.text == p.rows[2].sub && w.rgb == refwin::kMuted;
            kind |= w.text == "предмет" && w.align == refwin::Align::Right && w.rgb == refwin::kFaint;
            group |= w.text == "ПРЕДМЕТИ · 1";
        }
        CHECK(name && sub && kind && group);
        // the picked result gold, its bar at its left; the one under the pointer only lighter
        panel::Picture picked, over;
        refwin::paint(picked, L, p, rows, 0, total, look);
        refwin::Look hover = look;
        hover.selected = -1;
        hover.hover = refwin::Under{refwin::Part::Row, 2};
        refwin::paint(over, L, p, rows, 0, total, hover);
        const int y = L.bodyY + rows[2].y + rows[2].h / 2;
        const auto at = [&](const panel::Picture& pic, int x) {
            return pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
        };
        const int band = rows[2].x + rows[2].w - int(40 * k), bar = rows[2].x + 1;
        CHECK(((at(picked, band) >> 16) & 0xFF) > (at(picked, band) & 0xFF) + 4);  // gold: more red than blue
        CHECK(((at(over, band) >> 16) & 0xFF) <= (at(over, band) & 0xFF) + 4);
        CHECK(((at(picked, bar) >> 16) & 0xFF) > 150);
        // the query's mark: gold under «Wing»
        const float markX = float(rows[2].x) + 48 * k + L.measure("Fly ", 14.5f * k, true) + 6 * k;
        const int markY = L.bodyY + rows[2].y + int(16.5f * k);
        const uint32_t mark = hover.selected < 0 ? over.px[size_t(markY) * size_t(over.width) + size_t(markX)] : 0;
        CHECK(((mark >> 16) & 0xFF) > (mark & 0xFF) + 8);
        // a result's band is its own box; its repaint as the whole
        const Rect a = refwin::rowArea(L, p, rows, 0, 2);
        CHECK(a.left <= rows[2].x && a.right >= rows[2].x + rows[2].w);
        panel::Picture partial = picked;
        for (const refwin::Look* l : {&look, &hover})
            for (const Rect& r : refwin::hotAreas(L, p, rows, 0, *l))
                refwin::repaint(partial, L, p, rows, 0, total, hover, r);
        CHECK(partial.px == over.px);
    }
}

// 3.0.11-beta.30 (the owner: «нету кнопки перейти на главный экран справочника где все иконками и чтобы можно было
// выйти оттуда например сразу на карту мира»): «Головна» in the header (and its title) on every page but the first
// screen - the world map's too; its hover painted again alone
TEST(reference_home_button) {
    const refindex::Index ix = index();
    const refpages::Page card = refpages::mob(ix, Lang::Uk, 1002, ctx(""));
    for (const float k : {1.0f, 2.0f}) {
        const refwin::Layout L = refwin::layout(int(1920 * k), int(1080 * k), k);
        refwin::Texts t;
        t.title = "Довідник";
        t.back = "Назад до Довідник";
        const refwin::Chrome none = refwin::chrome(L, card, t);
        CHECK(none.home.w == 0);
        CHECK(refwin::under(L, card, {}, 0, none, none.title.x + 4, none.title.y + 10).part != refwin::Part::Home);
        t.home = true;
        const refwin::Chrome c = refwin::chrome(L, card, t);
        CHECK(c.home.w > 0 && c.home.x > c.title.x + c.title.w && c.back.x > c.home.x + c.home.w);
        int total = 0;
        const std::vector<refwin::Placed> rows = refwin::place(L, card, total);
        CHECK(refwin::under(L, card, rows, 0, c, c.home.x + 5, c.home.y + 5).part == refwin::Part::Home);
        CHECK(refwin::under(L, card, rows, 0, c, c.title.x + 4, c.title.y + 10).part == refwin::Part::Home);
        refwin::Look was;
        was.chrome = c;
        refwin::Look now = was;
        now.hover = refwin::Under{refwin::Part::Home, -1};
        panel::Picture partial;
        refwin::paint(partial, L, card, rows, 0, total, was);
        for (const refwin::Look* l : {&was, &now})
            for (const Rect& a : refwin::hotAreas(L, card, rows, 0, *l))
                refwin::repaint(partial, L, card, rows, 0, total, now, a);
        panel::Picture whole;
        refwin::paint(whole, L, card, rows, 0, total, now);
        CHECK(partial.px == whole.px);
        CHECK(!refwin::hotAreas(L, card, rows, 0, now).empty());
    }
}

// 3.0.11-beta.32: the monsters' list - those met on maps first, by their levels, each its picture and its line (the
// events' and summons' copies apart)
TEST(reference_mobs_list_groups) {
    const refindex::Index ix = index();
    const refpages::Page p = refpages::section(ix, Lang::Uk, refpages::Mobs);
    CHECK(p.rows[0].style == refpages::Style::Heading && p.rows[0].left == "На картах · 2");
    const refpages::Row* poring = rowWith(p, "Poring");
    CHECK(poring && poring->icon == "mob:1002" && poring->right == "Lv 1 · 2 карти");
    const refpages::Row* morroc = rowWith(p, "Satan Morroc");
    CHECK(morroc && morroc->gold && morroc->right == "MVP · Lv 99 · 1 карта");
    refindex::Index more = index();
    more.mobs[1999] = refindex::Mob{};
    more.mobs[1999].name = "Event Copy";
    const refpages::Page split = refpages::section(more, Lang::Uk, refpages::Mobs);
    CHECK(indexOf(split, "Не трапляються на картах (події, виклики) · 1") != size_t(-1));
}

// 3.0.11-beta.32 (the owner: «есть еще куча подобных мелочей, посмотри по ним еще»): the lists' small things - the
// hours past an hour, the counts in their words, the towns among the locations, the cards in three (the enchantments
// apart, their numbers in order), the pets told apart (their levels, no «#0» lure)
TEST(reference_lists_polish) {
    CHECK_EQ(refpages::respawn(117 * 60000, Lang::Uk), std::string("1 год 57 хв"));
    CHECK_EQ(refpages::respawn(90 * 60000, Lang::Uk), std::string("1 год 30 хв"));
    CHECK_EQ(refpages::respawn(60 * 60000, Lang::Uk), std::string("1 год"));
    CHECK_EQ(refpages::respawn(59 * 60000, Lang::Uk), std::string("59 хв"));
    refindex::Index ix = index();
    const auto starting = [](const refpages::Page& p, const std::string& head) -> const refpages::Row* {
        for (const refpages::Row& r : p.rows)
            if (r.left.rfind(head, 0) == 0) return &r;
        return nullptr;
    };
    const refpages::Page services = refpages::section(ix, Lang::Uk, refpages::Services);
    const refpages::Row* tools = starting(services, "Tool Dealer · Prontera");
    CHECK(tools && tools->right == "2 товари");
    const refpages::Page maps = refpages::section(ix, Lang::Uk, refpages::Maps);
    CHECK(maps.rows.size() > 2 && maps.rows[0].left == "Міста · 1" && maps.rows[0].style == refpages::Style::Heading);
    const refpages::Row* prontera = starting(maps, "Prontera City");
    CHECK(prontera && prontera->right == "Kafra · 1 крамниця");
    const refpages::Row* field = starting(maps, "Prontera Field");
    CHECK(field && field->right == "1 вид монстрів");
    CHECK(indexOf(maps, "З монстрами · 3") != size_t(-1));
    const refpages::Page home = refpages::home(ix, Lang::Uk, refpages::Context{});
    const refpages::Row* places = rowWith(home, "Локації");
    CHECK(places && places->right == "4");  // the towns counted too
    {  // a guild's castle and a passage have their Kafra too - not towns
        refindex::Index more = index();
        refindex::Npc kafra;
        kafra.name = "Kafra Staff";
        kafra.kind = refindex::NpcKind::Kafra;
        more.maps["gefg_cas01"].npcs.push_back(kafra);
        more.maps["gefg_cas01"].flags = {"gvg_castle", "nomemo"};
        more.maps["izlu2dun"].npcs.push_back(kafra);
        more.maps["aldeba_in"].npcs.push_back(kafra);
        more.maps["aldeba_in"].flags = {"indoors"};
        more.maps["brasilis"].npcs.push_back(kafra);
        const refpages::Page towns = refpages::section(more, Lang::Uk, refpages::Maps);
        CHECK(towns.rows[0].left == "Міста · 2" && starting(towns, "brasilis") && !starting(towns, "gefg_cas01"));
        CHECK(!starting(towns, "izlu2dun") && !starting(towns, "aldeba_in"));
        CHECK(towns.rows[1].left == "brasilis");  // among the names whatever their letters' case
    }
    // the cards: dropped, not dropped, the enchantments - «AGI+2» before «AGI+10»
    ix.items[4002] = refindex::Item{"Fabre Card", "Fabre_Card", 6};
    ix.items[4700] = refindex::Item{"AGI+10", "Agility10", 6};
    ix.items[4701] = refindex::Item{"AGI+2", "Agility2", 6};
    const refpages::Page cards = refpages::section(ix, Lang::Uk, refpages::Cards);
    CHECK(indexOf(cards, "Випадають з монстрів · 1") != size_t(-1));
    CHECK(indexOf(cards, "Не випадають з монстрів (події, квести) · 1") != size_t(-1));
    CHECK(indexOf(cards, "Зачарування · 2") != size_t(-1));
    CHECK(indexOf(cards, "AGI+2") < indexOf(cards, "AGI+10") && indexOf(cards, "AGI+10") != size_t(-1));
    CHECK(indexOf(cards, "Poring Card") < indexOf(cards, "Fabre Card"));
    // the pets: their levels, their pictures, no lure when there is none
    ix.pets.push_back(refindex::Pet{1917, 0, 0, 0, 0});
    const refpages::Page pets = refpages::section(ix, Lang::Uk, refpages::Pets);
    const refpages::Row* poring = rowWith(pets, "Poring");
    CHECK(poring && poring->right == "Lv 1 · приманка: Apple" && poring->icon == "mob:1002");
    const refpages::Row* morroc = rowWith(pets, "Satan Morroc");
    CHECK(morroc && morroc->right == "Lv 99");
}
