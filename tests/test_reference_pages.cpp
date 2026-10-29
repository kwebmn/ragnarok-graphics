// 3.0.11-beta.7: «Довідник» - its pages from the reference pack (core/reference_pages) and its window's shapes
// (core/reference_window): what a card says and where its rows lead, the words for numbers, the layout and the clicks.
#include <string>

#include "core/reference_live.hpp"
#include "core/reference_pages.hpp"
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
    CHECK_EQ(refpages::elementName(1, Lang::Uk), std::string("Вода"));
    CHECK_EQ(refpages::raceName(3, Lang::En), std::string("Plant"));
    CHECK_EQ(refpages::typeName(6, Lang::Uk), std::string("Картка"));
}

TEST(reference_pages_home_and_search) {
    const refindex::Index ix = index();
    const refpages::Page home = refpages::home(ix, Lang::Uk);
    CHECK_EQ(home.rows.size(), size_t(refpages::kSections) + 2);  // the tiles, the world map's, the keys' line
    CHECK(home.rows.back().style == refpages::Style::Note);
    CHECK(home.rows[0].style == refpages::Style::Tile && home.rows[0].left == "Монстри · 2");
    CHECK(home.rows[0].link.go == refpages::Go::Section && home.rows[0].link.id == refpages::Mobs);
    const refpages::Page found = refpages::found(ix, Lang::Uk, "por");
    CHECK(found.rows[0].style == refpages::Style::Heading);
    const refpages::Row* poring = rowWith(found, "Poring");
    CHECK(poring && poring->link.go == refpages::Go::Mob && poring->link.id == 1002);
    CHECK(rowWith(found, "Poring Card") != nullptr);  // the items too
    CHECK(rowWith(refpages::found(ix, Lang::Uk, "prontera city"), "Prontera City (prontera)") != nullptr);  // a map
    const refpages::Page none = refpages::found(ix, Lang::Uk, "nothing like it");
    CHECK(none.rows.size() == 1 && none.rows[0].style == refpages::Style::Note);
}

TEST(reference_pages_a_monsters_card) {
    const refindex::Index ix = index();
    const refpages::Page p = refpages::mob(ix, Lang::Uk, 1002, ctx("prt_fild05"));
    CHECK(p.rows[0].style == refpages::Style::Title && p.rows[0].left == "Poring" && p.rows[0].right == "Lv 1");
    CHECK(p.rows[1].left.find("HP 50 · Вода 1 · Рослина · Середній") == 0);
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
    CHECK(p.rows[0].left == "Fly Wing" && p.rows[0].right == "Вживане");
    CHECK(p.rows[1].left == "вага 0,5 · ціна 60 z");
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
    CHECK(p.rows[0].left == "Prontera City" && p.rows[0].right == "prontera" && p.rows[0].gold);
    CHECK(p.rows[1].left == "Prontera City, Capitol of Rune-Midgard · місто · без запам'ятовування · ви тут");
    const size_t tp = indexOf(p, "Телепорти");
    CHECK_EQ(p.rows[tp].right, std::string("1"));  // two Kafra rows to the same place, once
    CHECK(p.rows[tp + 1].left == "Kafra → Izlude Town (izlude)" && p.rows[tp + 1].right == "600 z");
    CHECK(p.rows[tp + 1].link.map == "izlude");
    CHECK(rowWith(p, "Tool Dealer") != nullptr);
    const refpages::Row* kafra = rowWith(p, "Kafra Employee");
    CHECK(kafra && kafra->dot == 0xD6336C && kafra->right == "150, 180");
    const refpages::Page field = refpages::map(ix, Lang::Uk, "prt_fild08", ctx(""));
    const size_t exits = indexOf(field, "Виходи");
    CHECK_EQ(field.rows[exits].right, std::string("1"));  // two warps to Prontera, one exit
    CHECK(field.rows[exits + 1].link.map == "prontera");
}

TEST(reference_pages_long_sections_open_by_their_row) {
    // a monster on eight maps: the first five and «усі 8 ›», which opens the section in place
    std::string spawns;
    for (int i = 0; i < 8; ++i)
        spawns += std::string(i ? "," : "") + "\"m" + std::to_string(i) + "\":[[1,8,0,0,0,0,0]]";
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{"1":["A",1,1,0,0,0,0,0,1,0,[],[],null]},"spawns":{)" + spawns +
                              R"(},"routes":[],"npcs":[]})",
                          ix));
    const refpages::Page closed = refpages::mob(ix, Lang::Uk, 1, ctx("m6"));
    const size_t where = indexOf(closed, "Де знайти");
    CHECK_EQ(closed.rows[where].right, std::string("8"));
    CHECK(closed.rows[where + 1].link.map == "m6");  // the player's map first, before the cut
    const refpages::Row& more = closed.rows[where + 1 + refpages::kFew];
    CHECK(more.left == "усі 8 ›" && more.link.go == refpages::Go::Expand && more.link.id == refpages::Where);
    const refpages::Page open = refpages::mob(ix, Lang::Uk, 1, ctx("m6"), 1u << refpages::Where);
    CHECK_EQ(open.rows.size(), closed.rows.size() + 8 - refpages::kFew - 1);
    CHECK(rowWith(open, "усі 8 ›") == nullptr);
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
    CHECK(rows[0].y == rows[2].y && rows[3].y > rows[0].y && rows[1].x > rows[0].x);  // three tiles to a line
    CHECK(total > rows[8].y);
    // clicks: ×, the field, a tile; nothing outside
    CHECK(refwin::under(L, home, rows, 0, false, float(L.w - 10), 10).part == refwin::Part::Close);
    CHECK(refwin::under(L, home, rows, 0, false, 200, float(L.header + 10)).part == refwin::Part::Search);
    const refwin::Under tile = refwin::under(L, home, rows, 0, false, float(rows[4].x + 20),
                                             float(L.bodyY + rows[4].y + 20));
    CHECK(tile.part == refwin::Part::Row && tile.row == 4);
    CHECK(refwin::under(L, home, rows, 0, false, -5, 10).part == refwin::Part::None);
    CHECK(refwin::under(L, home, rows, 0, false, 30, 10).part != refwin::Part::Back);  // no «Назад» on the first page
    CHECK(refwin::under(L, home, rows, 0, true, 30, 10).part == refwin::Part::Back);
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
    t.query = "por";
    t.footer = "Пакет даних";
    const std::vector<refwin::Words> words = refwin::words(L, p, rows, 0, t, refwin::Look{});
    bool title = false, typed = false, poring = false;
    for (const refwin::Words& w : words) {
        title = title || w.text == "Довідник";
        typed = typed || w.text == "por|";
        poring = poring || (w.text == "Poring" && w.px == 24.0f);
        CHECK(w.box.x >= 0 && w.box.y >= 0 && w.box.x + w.box.w <= float(L.w) + 0.5f);
        CHECK(w.box.y + w.box.h <= float(L.h));
    }
    CHECK(title && typed && poring);
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
