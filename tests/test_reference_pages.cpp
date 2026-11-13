// 3.0.11-beta.7: «Довідник» - its pages from the reference pack (core/reference_pages) and its window's shapes
// (core/reference_window): what a card says and where its rows lead, the words for numbers, the layout and the clicks.
#include <algorithm>
#include <cmath>
#include <string>

#include "core/reference_live.hpp"
#include "core/bar_hint.hpp"
#include "core/reference_pages.hpp"
#include "core/reference_route.hpp"
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
    CHECK_EQ(home.rows.size(), size_t(15));
    CHECK(home.rows[0].style == refpages::Style::Search && home.rows[0].right == "Ctrl+F");
    CHECK(home.rows[1].style == refpages::Style::Hint);
    CHECK(home.rows[2].style == refpages::Style::Tile && home.rows[2].left == "Монстри");
    CHECK(home.rows[2].right == "2" && home.rows[2].icon == "ui:monster");
    CHECK_EQ(home.rows[2].sub, std::string("рівень, стихія, де водяться, що падає"));
    CHECK(home.rows[2].link.go == refpages::Go::Section && home.rows[2].link.id == refpages::Mobs);
    const char* names[] = {"Монстри", "Предмети", "Локації",     "Квести",
                           "NPC і послуги", "MVP", "Пети", "Крафт",
                           "Навички", "Карта світу", "Стихії"};  // the concept's order (3.0.18-beta.244: «Навички»)
    for (size_t i = 0; i < 11; ++i) CHECK_EQ(home.rows[2 + i].left, std::string(names[i]));
    CHECK(home.rows[10].link.go == refpages::Go::Section && home.rows[10].link.id == refpages::Skills);
    CHECK(home.rows[11].link.go == refpages::Go::World && home.rows[11].right.empty());
    CHECK(home.rows[13].style == refpages::Style::Block && home.rows[13].left == "Обране");
    CHECK_EQ(home.rows[13].share, 0);
    CHECK(home.rows[14].style == refpages::Style::Note);
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
    CHECK(p.rows[1].style == refpages::Style::Portrait && p.rows[1].picture.empty());
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
    // 3.0.18-beta.240: the NPCs beside the map by their kinds, each one's card
    const refpages::Row* kafra = nullptr;
    for (const refpages::Row& r : p.rows)
        if (r.side && r.left == "Kafra Employee") kafra = &r;
    CHECK(kafra && kafra->dot == 0xE0447C && kafra->right == "150, 180");
    CHECK(kafra && kafra->link.go == refpages::Go::Npc && kafra->link.map == "prontera" && kafra->link.id == 0);
    const refpages::Page izlude = refpages::map(ix, Lang::Uk, "izlude", ctx(""));  // none there: by the teleport
    const size_t far = indexOf(izlude, "Kafra поруч");
    CHECK(far == size_t(-1));  // the way back is not known to the pack: no Kafra found
    const refpages::Page field = refpages::map(ix, Lang::Uk, "prt_fild08", ctx(""));
    const size_t exits = indexOf(field, "Виходи");
    CHECK_EQ(field.rows[exits].right, std::string("1"));  // two warps to Prontera, one exit
    CHECK(field.rows[exits + 1].link.map == "prontera");
}

TEST(reference_pages_long_sections_open_by_their_row) {
    // 3.0.18-beta.239 (the owner 13.11: «всегда выводить полный список, только когда там больше 100 значений»): a
    // monster on 130 maps - the first 100 and «усі 130 ›», which opens the section in place; on 100 - all of them
    const auto onMaps = [](int maps) {
        std::string spawns;
        for (int i = 0; i < maps; ++i)
            spawns += std::string(i ? "," : "") + "\"m" + std::to_string(i) + "\":[[1,8,0,0,0,0,0]]";
        return spawns;
    };
    refindex::Index all;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{"1":["A",1,1,0,0,0,0,0,1,0,[],[],null]},"spawns":{)" +
                              onMaps(100) + R"(},"routes":[],"npcs":[]})",
                          all));
    CHECK(rowWith(refpages::mob(all, Lang::Uk, 1, ctx("m6")), "усі 100 ›") == nullptr);
    const std::string spawns = onMaps(130);
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{"1":["A",1,1,0,0,0,0,0,1,0,[],[],null]},"spawns":{)" + spawns +
                              R"(},"routes":[],"npcs":[]})",
                          ix));
    const refpages::Page closed = refpages::mob(ix, Lang::Uk, 1, ctx("m6"));
    const size_t where = indexOf(closed, "Де знайти");
    CHECK_EQ(closed.rows[where].right, std::string("130"));
    CHECK(closed.rows[where + 1].link.map == "m6");  // the player's map first, before the cut
    CHECK_EQ(refpages::kFew, size_t(100));
    const refpages::Row& more = closed.rows[where + 1 + refpages::kFew];
    CHECK(more.left == "усі 130 ›" && more.link.go == refpages::Go::Expand && more.link.id == refpages::Where);
    const refpages::Page open = refpages::mob(ix, Lang::Uk, 1, ctx("m6"), 1u << refpages::Where);
    CHECK_EQ(open.rows.size(), closed.rows.size() + 130 - refpages::kFew - 1);
    CHECK(rowWith(open, "усі 130 ›") == nullptr);
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
    CHECK(found.rows[npcs + 1].link.go == refpages::Go::Npc && found.rows[npcs + 1].link.map == "prontera");
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
    CHECK_EQ(tables.name, std::string("Стихії"));
    CHECK(indexOf(tables, "Стихії атаки проти захисту рівня 1") != size_t(-1));
    CHECK(indexOf(tables, "Стихії атаки проти захисту рівня 4") != size_t(-1));
    const refpages::Row* fire = rowWith(tables, "Fire");  // 3.0.11-beta.32: a grid - fire against earth 150, water 50
    CHECK(fire && fire->style == refpages::Style::Grid && fire->tags.size() == 10);
    CHECK(fire && fire->tags[2] == "150" && fire->tags[1] == "50" && fire->tags[0] == "100");
    CHECK(tables.rows[1].style == refpages::Style::Grid && tables.rows[1].gold && tables.rows[1].tags[3] == "Fire");
    CHECK(tables.rows[1].tags[0] == "Neutral");  // the columns' whole names: the window cuts them only when narrow
}

// 3.0.11-beta.26, stage 2: the concept's cards (v23_ref_monster) - the rail of sections at the left, the card's head,
// its two main blocks side by side, the rest in sections that fold
TEST(reference_concept_cards) {
    const refindex::Index ix = index();
    const refpages::Link poringAt{refpages::Go::Mob, 1002, ""};
    refpages::Page p = refpages::withRail(refpages::mob(ix, Lang::Uk, 1002, ctx("prontera")), ix, Lang::Uk, poringAt);
    CHECK_EQ(p.rail.size(), size_t(12));  // 3.0.11-beta.30: «Головна» and all the first screen's sections
    CHECK(p.rail[0].left == "Головна" && p.rail[0].icon == "ui:home" && p.rail[0].link.go == refpages::Go::None);
    CHECK(p.rail[10].left == "Карта світу" && p.rail[10].link.go == refpages::Go::World);
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
    // a folded section: its heading alone, folded
    refpages::Context folded = ctx("prontera");
    folded.folded = 1u << refpages::Elements;
    const refpages::Page shut =
        refpages::withRail(refpages::open(ix, Lang::Uk, poringAt, folded), ix, Lang::Uk, poringAt);
    const size_t at = indexOf(shut, "Шкода стихіями");
    CHECK(at != size_t(-1) && shut.rows[at].closed);
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
        else
            ix.craftOf[100 + i].push_back(0);  // 3.0.18-beta.250: the rest crafted - things one can get, unsaid
    }
    const refpages::Page p = refpages::found(ix, Lang::Uk, "stone");
    CHECK(p.rows[1].style == refpages::Style::Group && p.rows[1].left == "Предмети · 30");
    CHECK(p.rows[2].left == "Stone Ad" && p.rows[2].sub == "10 z · 21 крамниця");  // the most shops first
    CHECK_EQ(p.rows[3].sub, std::string("10 z · 5 крамниць"));
    CHECK_EQ(p.rows[4].sub, std::string("10 z · 3 крамниці"));
    CHECK_EQ(p.rows[5].sub, std::string("10 z · 1 крамниця"));
    CHECK(p.rows[6].sub.empty());  // «Інше» unsaid, nothing else to say
    // 3.0.18-beta.239 (the owner 13.11): a group whole up to 100 - all thirty stones, no «ще …»
    CHECK(p.rows[31].style == refpages::Style::Result && p.rows[32].style == refpages::Style::Group);
    CHECK(p.rows[32].left.rfind("Навички · ", 0) == 0 && p.rows.back().style == refpages::Style::Hint);  // .244
    refindex::Index many;  // 130 stones: the search's own cut - the first 100 and «type more of the name»
    for (int i = 0; i < 130; ++i) {
        many.items[1000 + i] = refindex::Item{"Stone " + std::to_string(1000 + i), "S", 3, 10, 1, 0};
        many.craftOf[1000 + i].push_back(0);  // 3.0.18-beta.250: things one can get
    }
    const refpages::Page cut = refpages::found(many, Lang::Uk, "stone");
    CHECK(cut.rows[101].style == refpages::Style::Result && cut.rows[101].left == "Stone 1099");
    CHECK(cut.rows[102].style == refpages::Style::Note && cut.rows[102].left == "перші 100 з 130 - уточніть назву");
    CHECK_EQ(refpages::found(ix, Lang::En, "stone aa").rows[2].sub, std::string("10 z · 1 shop"));
    for (int i = 125; i < 130; ++i) ix.items.erase(i);  // 25: all of them, no «ще 5 ›»
    const refpages::Page most = refpages::found(ix, Lang::Uk, "stone");
    CHECK(most.rows[26].style == refpages::Style::Result && most.rows[27].style == refpages::Style::Group);
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
    // 3.0.18-beta.250 (the owner: «убери мобов которых нету на картах, они отвлекают»): none off the maps - not in the
    // list, not in the search, not counted on the first screen; its card still opens by a link
    refindex::Index more = index();
    more.mobs[1999] = refindex::Mob{};
    more.mobs[1999].name = "Event Copy";
    const refpages::Page split = refpages::section(more, Lang::Uk, refpages::Mobs);
    CHECK(rowWith(split, "Event Copy") == nullptr);
    for (const refpages::Row& r : split.rows) CHECK(r.left.find("Не трапляються") == std::string::npos);
    CHECK(rowWith(refpages::found(more, Lang::Uk, "event copy"), "Event Copy") == nullptr);
    const refpages::Row* tile = rowWith(refpages::home(more, Lang::Uk), "Монстри");
    CHECK(tile && tile->right == "2");
    CHECK(!refpages::mob(more, Lang::Uk, 1999, ctx("")).rows.empty());
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
    for (const int sold : {4002, 4700, 4701}) ix.soldAt[sold].emplace_back(0, 1000);  // .250: things one can get
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

// 3.0.11-beta.69 (the concept's question 21: the data's states and «Подробиці» by a click): the page the bottom's
// chips open - the pack (its date and server, «може бути застарілим» past 14 days) and the live data (fresh, stale
// with its age, none yet); the chips' words the same states
TEST(reference_data_states) {
    const refindex::Index ix = index();  // the pack of 2026-10-23
    refpages::Context c = ctx("");
    c.now = 1793361600;  // 2026-10-30 12:00 UTC: a week on
    c.liveAt = c.now - 120;
    const refpages::Link at{refpages::Go::Section, refpages::kDataSection, ""};
    const auto has = [](const refpages::Page& p, const std::string& words) {
        return std::any_of(p.rows.begin(), p.rows.end(), [&](const refpages::Row& r) {
            return r.left.find(words) != std::string::npos || r.right.find(words) != std::string::npos;
        });
    };
    const refpages::Page fresh = refpages::open(ix, Lang::Uk, at, c);
    CHECK_EQ(fresh.name, std::string("Дані довідника"));
    CHECK(has(fresh, "2026-10-23") && has(fresh, "d68ea17e") && has(fresh, "оновлено 2 хв тому"));
    CHECK(!has(fresh, "застаріл"));
    c.liveAt = c.now - 3 * 3600;  // the link gone 3 hours ago: the last, its age, may be out of date
    const refpages::Page stale = refpages::open(ix, Lang::Uk, at, c);
    CHECK(has(stale, "3 год тому") && has(stale, "можуть бути застарілими"));
    c.liveAt = 0;  // none came
    CHECK(has(refpages::open(ix, Lang::Uk, at, c), "ще не приходили"));
    c.now = 1794312000;  // 2026-11-10: the pack 18 days old
    CHECK(has(refpages::open(ix, Lang::Uk, at, c), "може бути застарілим"));
    CHECK(refpages::packStale(ix, c.now) && !refpages::packStale(ix, 1793361600));
}

// 3.0.11-beta.70 (the concept: «что закрепить (квест, путь, вещь для сбора)»): an item monsters drop - its card's pin
// for collecting it, at its end; one no monster drops: none
TEST(reference_pages_track_an_item) {
    const refindex::Index ix = index();
    refpages::Context c = ctx("");
    const refpages::Row pin = refpages::item(ix, Lang::Uk, 909, c).rows.back();  // Jellopy: Poring drops it
    CHECK(pin.style == refpages::Style::Button && pin.left == "Відстежувати збір" && !pin.gold);
    CHECK(pin.link.go == refpages::Go::Track && pin.link.id == 1 && pin.link.map == "909");
    const std::vector<int> tracked{909};
    c.trackedItems = &tracked;
    const refpages::Row on = refpages::item(ix, Lang::Uk, 909, c).rows.back();
    CHECK(on.left == "\xE2\x9C\x93 Збір відстежується" && on.gold);
    CHECK(ix.droppedBy.find(1101) == ix.droppedBy.end());  // a Sword: no monster drops it
    const refpages::Page none = refpages::item(ix, Lang::Uk, 1101, c);
    CHECK(!none.rows.empty() && none.rows.back().link.go != refpages::Go::Track);
}

// 3.0.11-beta.70: the tracker's entry of an item to collect - who drops it most, at what rate
TEST(reference_pages_tracked_item_entry) {
    const refindex::Index ix = index();
    const std::vector<tracker::Entry> e = refpages::trackedEntries(ix, Lang::Uk, {}, {909, 999999});
    CHECK_EQ(e.size(), size_t(1));
    if (e.empty()) return;
    CHECK(e[0].kind == tracker::Kind::Item && e[0].title == "Jellopy" && e[0].tag == "збір");
    CHECK(!e[0].lines.empty() && e[0].lines[0].text == "Poring" && e[0].lines[0].right == "70 %");
}

// 3.0.18-beta.239: a merged chance's note - how it was made («8 × 1 %», «11 % + 8 %», the biggest first)
TEST(reference_pages_chance_parts) {
    CHECK_EQ(refpages::chanceParts(std::vector<int>(8, 100), Lang::Uk), std::string("8 × 1 %"));
    CHECK_EQ(refpages::chanceParts({800, 1100}, Lang::Uk), std::string("11 % + 8 %"));
    CHECK_EQ(refpages::chanceParts({150, 1000}, Lang::En), std::string("10 % + 1.5 %"));
    CHECK_EQ(refpages::chanceParts({500}, Lang::Uk), std::string());
}

// 3.0.18-beta.239: one row a monster in an item's droppers and one a thing in a monster's drops, with the note
TEST(reference_pages_a_merged_drop_says_its_parts) {
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"items":{"512":["Apple","Apple",0,15,2,0,0]},)"
                          R"("mobs":{"1284":["Hugeling",1,1,0,0,0,0,0,1,0,)"
                          R"([[512,100],[512,100],[512,100],[512,100],[512,100],[512,100],[512,100],[512,100]],)"
                          R"([],null]},"spawns":{"prt_fild08":[[1284,1,0,0,0,0,0]]},"routes":[],"npcs":[]})",
                          ix));
    const refpages::Page apple = refpages::item(ix, Lang::Uk, 512, ctx(""));
    int hugelings = 0;
    for (const refpages::Row& r : apple.rows)
        if (r.left == "Hugeling") {
            ++hugelings;
            CHECK(r.right.rfind("7,73 %", 0) == 0);
            CHECK_EQ(r.sub, std::string("8 × 1 %"));
        }
    CHECK_EQ(hugelings, 1);
    const refpages::Page mob = refpages::mob(ix, Lang::Uk, 1284, ctx(""));
    const refpages::Row* row = rowWith(mob, "Apple");
    CHECK(row != nullptr && row->right == "7,73 %" && row->sub == "8 × 1 %");
}

// 3.0.18-beta.240 (the owner 13.11, «Карты: что на них»): a location's card - the map (one with «⤢»), beside it the
// NPCs by kinds (Kafra, merchants, quests ... - the site's folders), each row its NPC's card; the old flat list gone
TEST(reference_pages_a_maps_card_map_and_npcs_by_kind) {
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{},"spawns":{},"routes":[],"shops":[],"npcs":[)"
                          R"(["prontera",150,180,"Kafra Employee",113,"kafra"],["prontera",100,120,"Old Man",86,"quest"],)"
                          R"(["prontera",104,49,"Fruit Gardener",83,"merchant"],["prontera",20,20,"Lady",90,"city"]]})",
                          ix));
    const refpages::Page p = refpages::map(ix, Lang::Uk, "prontera", ctx(""));
    const size_t view = [&] {
        for (size_t i = 0; i < p.rows.size(); ++i)
            if (p.rows[i].style == refpages::Style::MapView) return i;
        return size_t(-1);
    }();
    CHECK(view != size_t(-1) && p.rows[view].left == "prontera");
    std::vector<std::string> groups;
    for (const refpages::Row& r : p.rows)
        if (r.side && r.style == refpages::Style::Group) groups.push_back(r.left);
    CHECK((groups == std::vector<std::string>{"Кафра · 1", "Торговці й послуги · 1", "Квести · 1",
                                              "Міські жителі · 1"}));
    const refpages::Row* oldMan = rowWith(p, "Old Man");
    CHECK(oldMan && oldMan->side && oldMan->link.go == refpages::Go::Npc && oldMan->dot == 0xF08C00);
    CHECK(indexOf(p, "NPC") == size_t(-1));  // the old flat list
}

// 3.0.18-beta.240: an NPC's card - its kind and place, «Вести сюди» to its very cell, its map with it in the middle,
// a merchant's goods with their prices, Kafra's teleports
TEST(reference_pages_an_npcs_card) {
    const refindex::Index ix = index();  // a Tool Dealer's shop at prontera 134, 203; Kafra at 150, 180
    refindex::Index shopIx = ix;
    shopIx.maps["prontera"].npcs.push_back(refindex::Npc{134, 203, "Tool Dealer", 83, refindex::NpcKind::Tool});
    const int dealer = int(shopIx.maps["prontera"].npcs.size()) - 1;
    const refpages::Page p = refpages::open(shopIx, Lang::Uk, refpages::Link{refpages::Go::Npc, dealer, "prontera"},
                                            ctx(""));
    CHECK(!p.rows.empty() && p.rows[0].style == refpages::Style::Title && p.rows[0].left == "Tool Dealer");
    const refpages::Row* lead = rowWith(p, "Вести сюди ›");
    CHECK(lead && lead->link.go == refpages::Go::Follow && lead->link.id == 2 && lead->link.map == "prontera:134:203");
    const refpages::Row* onMap = rowWith(p, "Показати на карті ›");
    CHECK(onMap && onMap->link.go == refpages::Go::Map && onMap->link.map == "prontera" && onMap->link.id == dealer + 1);
    const refpages::Link* x = refpages::leadOf(p);  // 3.0.18-beta.241: the pad's X
    CHECK(x && x->go == refpages::Go::Follow && x->id == 2 && x->map == "prontera:134:203");
    const refpages::Row* wing = rowWith(p, "Fly Wing");
    CHECK(wing && wing->right == "60 z" && wing->link.go == refpages::Go::Item);
    const refpages::Row* hero = nullptr;  // kind, map and cell joined by a middle dot (UTF-8), not a double-encoded one
    for (const refpages::Row& r : p.rows)
        if (r.style == refpages::Style::Portrait) hero = &r;
    CHECK(hero && hero->left.find(" \xC2\xB7 ") != std::string::npos &&
          hero->left.find("\xC3\x82") == std::string::npos);
    const refpages::Page k = refpages::open(ix, Lang::Uk, refpages::Link{refpages::Go::Npc, 0, "prontera"}, ctx(""));
    CHECK(!k.rows.empty() && k.rows[0].left == "Kafra Employee");
    CHECK(rowWith(k, "Telemarketing") == nullptr);
    CHECK_EQ(refpages::titleOf(ix, Lang::Uk, refpages::Link{refpages::Go::Npc, 0, "prontera"}),
             std::string("Kafra Employee"));
}

// 3.0.18-beta.240: an NPC found by the search opens its own card
TEST(reference_search_an_npc_opens_its_card) {
    const refindex::Index ix = index();
    const refpages::Page f = refpages::found(ix, Lang::Uk, "kafra employee");
    const refpages::Row* r = nullptr;
    for (const refpages::Row& x : f.rows)
        if (x.style == refpages::Style::Result && x.left == "Kafra Employee") r = &x;
    CHECK(r && r->link.go == refpages::Go::Npc && r->link.map == "prontera" && r->link.id == 0);
}

// 3.0.18-beta.240 (the owner 13.11, question 8): «NPC і послуги» by the maps' kinds - each kind's count on all maps,
// its page the NPCs of that kind on every map, each its card
TEST(reference_pages_services_by_kind) {
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{},"spawns":{},"routes":[],"shops":[],"npcs":[)"
                          R"(["prontera",150,180,"Kafra Employee",113,"kafra"],["izlude",120,110,"Kafra Employee",113,)"
                          R"("kafra"],["prontera",100,120,"Old Man",86,"quest"]]})",
                          ix));
    const refpages::Page s = refpages::section(ix, Lang::Uk, refpages::Services);
    const size_t at = indexOf(s, "За видами");
    CHECK(at != size_t(-1));
    const refpages::Row* kafra = rowWith(s, "Кафра");
    CHECK(kafra && kafra->right == "2" && kafra->link.go == refpages::Go::Section &&
          kafra->link.id == refpages::kNpcKinds + int(refindex::NpcGroup::Kafra));
    const refpages::Page list = refpages::section(ix, Lang::Uk, kafra ? kafra->link.id : 0);
    int rows = 0;
    for (const refpages::Row& r : list.rows)
        if (r.link.go == refpages::Go::Npc) ++rows;
    CHECK_EQ(rows, 2);
    const refpages::Page railed = refpages::withRail(list, ix, Lang::Uk, kafra ? kafra->link : refpages::Link{});
    CHECK(railed.railOn >= 0 && railed.rail[size_t(railed.railOn)].left == "NPC і послуги");
}

// 3.0.18-beta.241 (the pad's LB / RB): the rail's next and previous entries, round; none on - the first or the last
TEST(reference_pages_rail_steps_round) {
    refpages::Page p;
    p.rail.resize(4);
    CHECK_EQ(refpages::railStep(p, +1), 0);
    CHECK_EQ(refpages::railStep(p, -1), 3);
    p.railOn = 2;
    CHECK_EQ(refpages::railStep(p, +1), 3);
    CHECK_EQ(refpages::railStep(p, -1), 1);
    p.railOn = 3;
    CHECK_EQ(refpages::railStep(p, +1), 0);
    p.railOn = 0;
    CHECK_EQ(refpages::railStep(p, -1), 3);
    p.rail.clear();
    p.railOn = -1;
    CHECK_EQ(refpages::railStep(p, +1), -1);
}

namespace {
const refpages::Row* portraitOf(const refpages::Page& p) {
    for (const refpages::Row& r : p.rows)
        if (r.style == refpages::Style::Portrait) return &r;
    return nullptr;
}
}  // namespace

// 3.0.18-beta.242 (the concept's cards): a boss's own timers (the HUD «Боси» panel's) first, the players' below; the
// bosses' list says the own one
TEST(reference_pages_own_boss_timers_first) {
    const refindex::Index ix = index();
    refpages::Context c = ctx("");
    c.now = 1800000000;
    reflive::Live live;
    live.timers.push_back(reflive::Timer{1917, "moc_fild20", c.now - 600});
    c.live = &live;
    std::vector<refpages::OwnTimer> own{{1917, "moc_fild20", c.now + 600}, {1002, "prt_fild08", c.now + 60}};
    c.own = &own;
    const refpages::Page p = refpages::mob(ix, Lang::Uk, 1917, c);
    const size_t mine = indexOf(p, "Ваші таймери"), theirs = indexOf(p, "Таймери гравців");
    CHECK(mine != size_t(-1) && theirs != size_t(-1) && mine < theirs);
    CHECK(mine != size_t(-1) && p.rows[mine].right == "1");
    CHECK(mine != size_t(-1) && p.rows[mine + 1].right == "через " + refpages::respawn(600000, Lang::Uk) &&
          !p.rows[mine + 1].gold && p.rows[mine + 1].link.go == refpages::Go::Map);
    own[0].back = c.now - 5;  // back by now
    const refpages::Page due = refpages::mob(ix, Lang::Uk, 1917, c);
    const size_t at = indexOf(due, "Ваші таймери");
    CHECK(at != size_t(-1) && due.rows[at + 1].right == "вже має бути" && due.rows[at + 1].gold);
    const refpages::Row* boss = rowWith(refpages::section(ix, Lang::Uk, refpages::Bosses, c), "Satan Morroc");
    CHECK(boss && boss->right.find("вже має бути") != std::string::npos);
}

// 3.0.18-beta.242: an item's words from the game's table under its name; what the player has, where
TEST(reference_pages_item_description_and_what_you_have) {
    const refindex::Index ix = index();
    refpages::Context c = ctx("");
    c.describe = [](int id) {
        return id == 601 ? std::vector<std::string>{"A wing.", "Teleports you."} : std::vector<std::string>{};
    };
    const std::map<int, int> bag{{601, 3}}, cart{{601, 1}}, storage{{601, 12}, {512, 4}};
    c.bag = &bag;
    c.cart = &cart;
    c.storage = &storage;
    const refpages::Page wing = refpages::item(ix, Lang::Uk, 601, c);
    const refpages::Row* hero = portraitOf(wing);
    CHECK(hero && hero->sub == "A wing.\nTeleports you.");
    CHECK(hero && hero->right == "у сумці 3 · у візку 1 · на складі 12");
    const refpages::Page apple = refpages::item(ix, Lang::Uk, 512, c);
    CHECK(portraitOf(apple) && portraitOf(apple)->sub.empty() && portraitOf(apple)->right == "на складі 4");
    const refpages::Page jellopy = refpages::item(ix, Lang::Uk, 909, c);
    CHECK(portraitOf(jellopy) && portraitOf(jellopy)->right.empty());  // none anywhere
}

// 3.0.18-beta.242: «З чого зробити» says what the bag has of each, «Входить у крафт» what is missing
TEST(reference_pages_craft_have_and_missing) {
    const refindex::Index ix = index();
    refpages::Context c = ctx("");
    const std::map<int, int> none, two{{998, 2}};
    c.bag = &none;
    const refpages::Row* iron = rowWith(refpages::item(ix, Lang::Uk, 1101, c), "Iron");
    CHECK(iron && iron->right == "×1 · є 0");
    const refpages::Page ironCard = refpages::item(ix, Lang::Uk, 998, c);
    const refpages::Row* sword = rowWith(ironCard, "Sword");
    CHECK(sword && sword->right == "бракує 1");
    c.bag = &two;
    const refpages::Page swordCard = refpages::item(ix, Lang::Uk, 1101, c);
    CHECK(rowWith(swordCard, "Iron") && rowWith(swordCard, "Iron")->right == "×1 · є 2");
    const refpages::Page ironHave = refpages::item(ix, Lang::Uk, 998, c);
    CHECK(rowWith(ironHave, "Sword") && rowWith(ironHave, "Sword")->right == "є все");
    c.bag = nullptr;  // not known: as before
    const refpages::Page plain = refpages::item(ix, Lang::Uk, 1101, c);
    CHECK(rowWith(plain, "Iron") && rowWith(plain, "Iron")->right == "×1");
    const refpages::Page plainIron = refpages::item(ix, Lang::Uk, 998, c);
    CHECK(rowWith(plainIron, "Sword") && rowWith(plainIron, "Sword")->right.empty());
}

// 3.0.18-beta.242 (the concept: monsters' levels coloured by the character's) - fitting within 10 levels, too strong
// above, too weak below; not known: none
TEST(reference_pages_monster_levels_by_yours) {
    const refindex::Index ix = index();
    refpages::Context c = ctx("");
    c.level = 50;
    const refpages::Page apple = refpages::item(ix, Lang::Uk, 512, c);  // Poring, Lv 1
    CHECK(rowWith(apple, "Poring") && rowWith(apple, "Poring")->lvTone == 3);
    const refpages::Page wing = refpages::item(ix, Lang::Uk, 601, c);  // Satan Morroc, Lv 99
    CHECK(rowWith(wing, "Satan Morroc") && rowWith(wing, "Satan Morroc")->lvTone == 2);
    c.level = 5;
    const refpages::Page near = refpages::item(ix, Lang::Uk, 512, c);
    CHECK(rowWith(near, "Poring") && rowWith(near, "Poring")->lvTone == 1);
    c.level = 0;
    const refpages::Page unknown = refpages::item(ix, Lang::Uk, 512, c);
    CHECK(rowWith(unknown, "Poring") && rowWith(unknown, "Poring")->lvTone == 0);
    c.level = 50;
    const refpages::Row* listed = rowWith(refpages::section(ix, Lang::Uk, refpages::Mobs, c), "Poring");
    CHECK(listed && listed->lvTone == 3);
    CHECK_EQ(refpages::levelTone(60, 50), 1);  // the edges: 10 above fits, 11 too strong
    CHECK_EQ(refpages::levelTone(61, 50), 2);
    CHECK_EQ(refpages::levelTone(40, 50), 1);
    CHECK_EQ(refpages::levelTone(39, 50), 3);
    CHECK_EQ(refpages::levelTone(10, 0), 0);
}

// 3.0.18-beta.244 («Довідник» › «Навички», the server's own tables): the jobs in groups, each with its own skills'
// count; a job's page - its own skills (not its earlier jobs'), each «Lv N · SP …», and the way to the earlier job's
TEST(reference_pages_skills_by_jobs) {
    const refindex::Index ix = index();
    const refpages::Page s = refpages::section(ix, Lang::Uk, refpages::Skills);
    CHECK_EQ(s.name, std::string("Навички"));
    CHECK(indexOf(s, "Перші професії") != size_t(-1) && indexOf(s, "Другі професії") != size_t(-1));
    CHECK(indexOf(s, "Перероджені") != size_t(-1));
    const refpages::Row* knight = rowWith(s, "Knight");
    CHECK(knight && knight->link.go == refpages::Go::Section && knight->link.id == refpages::kJobSkills + 7);
    CHECK(knight && !knight->right.empty());
    CHECK(rowWith(s, "Baby Knight") == nullptr);  // the babies', the mounted and the wedding's trees left out
    const refpages::Page sw = refpages::section(ix, Lang::Uk, refpages::kJobSkills + 1);  // Swordsman
    CHECK_EQ(sw.name, std::string("Swordsman"));
    const refpages::Row* bash = rowWith(sw, "Bash");
    CHECK(bash && bash->link.go == refpages::Go::Skill && bash->link.id == 5);
    CHECK(bash && bash->right.find("Lv 10") != std::string::npos && bash->right.find("SP 8") != std::string::npos);
    CHECK(rowWith(sw, "Basic Skill") == nullptr);  // the Novice's
    const refpages::Row* earlier = rowWith(sw, "Навички Novice ›");
    CHECK(earlier && earlier->link.id == refpages::kJobSkills + 0);
    const refpages::Page railed =
        refpages::withRail(sw, ix, Lang::Uk, refpages::Link{refpages::Go::Section, refpages::kJobSkills + 1, ""});
    CHECK(railed.railOn >= 0 && railed.rail[size_t(railed.railOn)].left == "Навички");
}

// 3.0.18-beta.244: a skill's card - its facts, the words (the site's or the game's), SP by level, what it needs, who
// learns it; its name in the search's group «Навички»
TEST(reference_pages_a_skills_card) {
    const refindex::Index ix = index();
    refpages::Context c;
    c.skillText = [](int id) {
        return id == 7 ? std::vector<std::string>{"Hits everything around."} : std::vector<std::string>{};
    };
    const refpages::Page p = refpages::open(ix, Lang::Uk, refpages::Link{refpages::Go::Skill, 7, ""}, c);
    CHECK(!p.rows.empty() && p.rows[0].left == "Magnum Break");
    const refpages::Row* hero = portraitOf(p);
    CHECK(hero && hero->picture == "skill:SM_MAGNUM" && hero->left.find("Lv 10") != std::string::npos);
    CHECK(hero && std::find(hero->tags.begin(), hero->tags.end(), "Фізична") != hero->tags.end());
    CHECK(hero && std::find(hero->tags.begin(), hero->tags.end(), "el3:Fire") != hero->tags.end());
    CHECK(rowWith(p, "Hits everything around.") != nullptr);
    const refpages::Row* bash = rowWith(p, "Bash");  // what it needs
    CHECK(bash && bash->right == "Lv 5" && bash->link.go == refpages::Go::Skill && bash->link.id == 5);
    const refpages::Row* who = rowWith(p, "Swordsman");
    CHECK(who && who->link.go == refpages::Go::Section && who->link.id == refpages::kJobSkills + 1);
    const refpages::Row* sp = rowWith(p, "Усі рівні");  // the same SP at every level
    CHECK(sp && sp->right == "SP 30");
    const refpages::Page bolt = refpages::open(ix, Lang::Uk, refpages::Link{refpages::Go::Skill, 19, ""}, c);
    const refpages::Row* first = rowWith(bolt, "Рівень 1");
    CHECK(first && first->right == "SP 12");
    CHECK_EQ(refpages::titleOf(ix, Lang::Uk, refpages::Link{refpages::Go::Skill, 7, ""}), std::string("Magnum Break"));
    const refpages::Page found = refpages::found(ix, Lang::Uk, "magnum");
    CHECK(indexOf(found, "Навички · 1") != size_t(-1));
    const refpages::Row* r = rowWith(found, "Magnum Break");
    CHECK(r && r->link.go == refpages::Go::Skill && r->link.id == 7);
}

// 3.0.18-beta.245 (the concept: Cyrillic typed by the layout): «зщкштп» is «poring» by the keys - found so, a line on
// top says it and offers the search as typed; as typed (the link's) the words themselves
TEST(reference_pages_search_typed_in_the_cyrillic_layout) {
    const refindex::Index ix = index();
    const refpages::Page p = refpages::found(ix, Lang::Uk, "зщкштп");
    CHECK(p.rows.size() > 2 && p.rows[1].left == "Набрано в українській розкладці — шукаю poring");
    CHECK(p.rows.size() > 2 && p.rows[1].right == "шукати як набрано ›" && p.rows[1].link.go == refpages::Go::AsTyped);
    CHECK(rowWith(p, "Poring") != nullptr);
    refpages::Context c;
    c.asTyped = true;
    const refpages::Page typed = refpages::found(ix, Lang::Uk, "зщкштп", c);
    CHECK(rowWith(typed, "Poring") == nullptr && typed.rows.back().style == refpages::Style::Empty);
    const refpages::Page ru = refpages::found(ix, Lang::Uk, "ыцщкв");  // ы: the Russian layout's «s» - «sword»
    CHECK(ru.rows.size() > 1 && ru.rows[1].left == "Набрано в російській розкладці — шукаю sword");
    const refpages::Page plain = refpages::found(ix, Lang::Uk, "poring");  // Latin: no line
    CHECK(plain.rows.size() > 1 && plain.rows[1].style == refpages::Style::Group);
}

// 3.0.18-beta.246 (the final review): a transcendent job's own skills are its own (not the second job's), and its
// earlier job is the second one
TEST(reference_pages_skills_of_a_transcendent_job) {
    const refindex::Index ix = index();
    const refpages::Page lk = refpages::section(ix, Lang::Uk, refpages::kJobSkills + 4008);  // Lord Knight
    CHECK_EQ(lk.name, std::string("Lord Knight"));
    CHECK(rowWith(lk, "Aura Blade") != nullptr);
    CHECK(rowWith(lk, "Bash") == nullptr && rowWith(lk, "Pierce") == nullptr);  // the Swordsman's, the Knight's
    const refpages::Row* earlier = rowWith(lk, "Навички Knight ›");
    CHECK(earlier && earlier->link.id == refpages::kJobSkills + 7);
}

// 3.0.18-beta.247 (R-ZW43KT «лагает справочник»: «Монстри» showed 1006 rows): a section's list past 100 rows shows its
// first 100 and «усі N ›» (the owner's 13.11 rule), opened it shows them all
TEST(reference_pages_long_sections_show_100_then_all) {
    refindex::Index ix;
    for (int i = 0; i < 150; ++i) {
        ix.mobs[2000 + i] = refindex::Mob{};
        ix.mobs[2000 + i].name = "Mob " + std::to_string(1000 + i);
        ix.mobs[2000 + i].lv = 1 + i;
        ix.spawnsOf[2000 + i].push_back(refindex::Where{"prt_fild08", 1, 0});
        ix.items[5000 + i] = refindex::Item{"Thing " + std::to_string(1000 + i), "T", 3, 10, 1, 0};
        ix.soldAt[5000 + i].emplace_back(0, 10);  // 3.0.18-beta.250: things one can get
    }
    const refpages::Page mobs = refpages::section(ix, Lang::Uk, refpages::Mobs);
    int shown = 0;
    for (const refpages::Row& r : mobs.rows) shown += r.link.go == refpages::Go::Mob;
    CHECK_EQ(shown, 100);
    const refpages::Row* all = rowWith(mobs, "усі 150 ›");
    CHECK(all && all->link.go == refpages::Go::Expand);
    const refpages::Page opened = refpages::section(ix, Lang::Uk, refpages::Mobs, {}, all ? 1u << all->link.id : 0u);
    shown = 0;
    for (const refpages::Row& r : opened.rows) shown += r.link.go == refpages::Go::Mob;
    CHECK_EQ(shown, 150);
    CHECK(rowWith(opened, "усі 150 ›") == nullptr);
    const refpages::Page etc = refpages::section(ix, Lang::Uk, 100 + 3);  // the items of one type
    shown = 0;
    for (const refpages::Row& r : etc.rows) shown += r.link.go == refpages::Go::Item;
    CHECK(shown == 100 && rowWith(etc, "усі 150 ›") != nullptr);
}

// 3.0.18-beta.250 (the owner: «и итемы»): a thing no one can get - no monster on a map drops it, no shop sells it, no
// craft makes it, no quest or pet takes it - not in the lists, not found, not counted; its card still opens by a link
TEST(reference_pages_things_no_one_can_get_are_not_listed) {
    refindex::Index ix = index();
    ix.items[7777] = refindex::Item{"Ghost Thing", "Ghost_Thing", 3, 10, 1, 0};
    CHECK(!refindex::obtainable(ix, 7777));
    CHECK(refindex::obtainable(ix, 512) && refindex::obtainable(ix, 1101) && refindex::obtainable(ix, 531));
    CHECK(rowWith(refpages::section(ix, Lang::Uk, 100 + 3), "Ghost Thing") == nullptr);
    CHECK(rowWith(refpages::found(ix, Lang::Uk, "ghost"), "Ghost Thing") == nullptr);
    CHECK(rowWith(refpages::found(ix, Lang::Uk, "ghost_thing"), "Ghost Thing") == nullptr);
    const refpages::Row* before = rowWith(refpages::home(index(), Lang::Uk), "Предмети");
    const refpages::Row* after = rowWith(refpages::home(ix, Lang::Uk), "Предмети");
    CHECK(before && after && before->right == after->right);
    CHECK(!refpages::item(ix, Lang::Uk, 7777, ctx("")).rows.empty());
}
