// 3.0.11-beta.8: the routes of «Довідник» - the maps' walkable parts (core/walk_grid) and the way over the pack's moves
// on them (core/reference_route), and «Як дістатися» in a map's card.
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "core/minimap_proto.hpp"
#include "core/reference_live.hpp"
#include "core/reference_pages.hpp"
#include "core/reference_route.hpp"
#include "core/reference_window.hpp"
#include "core/route_follow.hpp"
#include "core/walk_grid.hpp"
#include "core/world_map.hpp"
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

// town -> field -> tower (two floors, joined by stairs) -> cave; town -> isle by Kafra alone; a Guide in town who
// takes you to the cave if something (a quest) allows; Eden's officer in town, free, to the field or the isle
const char* kPack = R"({"v":1,"data":7,"commit":"c0ffee","date":"2026-10-29T01:00:00+03:00",
"items":{},"mobs":{},"spawns":{},
"routes":[["town",1,1,"field",1,1,"w","",null,0],["town",9,9,"field",9,8,"w","",null,0],
          ["field",1,2,"town",2,2,"w","",null,0],["field",8,8,"tower",1,1,"w","",null,0],
          ["tower",8,8,"cave",5,5,"w","",null,0],STAIRS
          ["town",5,5,"isle",5,5,"k","Kafra",1200,0],["town",3,3,"cave",1,1,"s","Guide",null,1],
          ["town",4,4,"field",5,5,"s","Eden",null,0],["town",4,4,"isle",4,4,"s","Eden",null,0]],
"shops":[],"npcs":[],"pets":[],"craft":[],"flags":{}})";

walkgrid::Grid open10() { return walkgrid::fromCells(10, 10, std::vector<uint8_t>(100, 1)); }

walkgrid::Grid tower() {  // a wall down x 5: the floor of x 0-4 and the one of x 6-9
    std::vector<uint8_t> cells(100, 1);
    for (int y = 0; y < 10; ++y) cells[size_t(y * 10 + 5)] = 0;
    return walkgrid::fromCells(10, 10, cells);
}

std::unique_ptr<refindex::Index> index(bool stairs = true) {
    std::string pack = kPack;
    pack.replace(pack.find("STAIRS"), 6, stairs ? R"(["tower",1,8,"tower",8,1,"w","",null,0],)" : "");
    auto ix = std::make_unique<refindex::Index>();
    CHECK(refindex::build(pack, *ix));
    auto grids = std::make_shared<walkgrid::Grids>();
    (*grids)["town"] = open10();
    (*grids)["field"] = open10();
    (*grids)["tower"] = tower();
    (*grids)["isle"] = open10();  // the cave's cells not known: the whole map one node
    ix->walk = grids;
    ix->graph = refroute::prepare(*ix);
    return ix;
}

std::string maps(const refroute::Route& r) {
    std::string s;
    for (const refroute::Step& st : r.steps) s += (s.empty() ? "" : " ") + st.move.to;
    return s;
}

}  // namespace

TEST(walk_grid_parts_runs_and_the_file) {
    const walkgrid::Grid t = tower();
    CHECK_EQ(t.partCount, 2);
    CHECK_EQ(int(t.at(0, 0)), 0);
    CHECK_EQ(int(t.at(9, 9)), 1);
    CHECK_EQ(int(t.at(5, 3)), int(walkgrid::kWall));
    CHECK_EQ(int(t.at(-1, 0)), int(walkgrid::kWall));
    CHECK_EQ(int(t.at(10, 0)), int(walkgrid::kWall));
    CHECK(t.around(5, 3, 0).empty());                         // on the wall, nothing that near
    CHECK(t.around(5, 3, 1) == std::vector<uint8_t>({0, 1}));  // both floors one cell away
    CHECK(t.around(2, 2, 15) == std::vector<uint8_t>({0}));    // the nearest ring only
    walkgrid::Grids maps;
    maps["tower"] = t;
    maps["field"] = open10();
    walkgrid::Grids back;
    CHECK(walkgrid::read(walkgrid::payload(maps), back));
    CHECK_EQ(back.size(), size_t(2));
    CHECK_EQ(back["tower"].w, 10);
    CHECK_EQ(back["tower"].partCount, 2);
    CHECK_EQ(int(back["tower"].at(7, 4)), 1);
    CHECK_EQ(int(back["field"].at(3, 3)), 0);
    std::string bad = walkgrid::payload(maps);
    bad.resize(bad.size() - 2);  // cut
    CHECK(!walkgrid::read(bad, back));
    CHECK(!walkgrid::read("not a grid file", back));
}

// 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти» - an experiment): the player's part (on a wall the one
// part next to it), a part's cells flat - what the minimap leaves lit
TEST(walk_grid_the_players_part_and_its_cells) {
    const walkgrid::Grid t = tower();  // a wall down x 5: the floors x 0-4 and x 6-9
    CHECK_EQ(int(walkgrid::partOf(t, 2, 3)), 0);
    CHECK_EQ(int(walkgrid::partOf(t, 8, 3)), 1);
    CHECK_EQ(int(walkgrid::partOf(t, 5, 3)), int(walkgrid::kWall));  // on the wall between both: not known
    std::vector<uint8_t> cells(100, 1);
    for (int y = 0; y < 10; ++y) cells[size_t(y * 10 + 5)] = cells[size_t(y * 10 + 9)] = 0;
    const walkgrid::Grid edge = walkgrid::fromCells(10, 10, cells);
    CHECK_EQ(int(walkgrid::partOf(edge, 9, 3)), int(edge.at(8, 3)));  // on the edge's wall: the one floor by it
    CHECK_EQ(int(walkgrid::partOf(edge, -4, 3)), int(walkgrid::kWall));
    const std::vector<uint8_t> mask = walkgrid::partMask(t, 0);
    CHECK_EQ(mask.size(), size_t(100));
    CHECK(mask[size_t(3 * 10 + 2)] == 1 && mask[size_t(3 * 10 + 8)] == 0 && mask[size_t(3 * 10 + 5)] == 0);
    CHECK_EQ(std::count(mask.begin(), mask.end(), uint8_t(1)), 50);
    CHECK(walkgrid::partMask(t, walkgrid::kWall).empty());
}

TEST(reference_route_walks_the_floors_by_the_stairs) {
    const auto ix = index();
    const refroute::Route r = refroute::find(*ix, "town", -1, -1, "cave", true);
    CHECK(r.found);
    CHECK_EQ(maps(r), std::string("field tower tower cave"));  // the Guide's way has a condition: not while another
    CHECK(!r.conditional && !r.paid && r.zeny == 0);
    CHECK_EQ(r.steps[2].from, std::string("tower"));          // the stairs, within the tower
    CHECK(refroute::find(*ix, "tower", 8, 3, "cave", true).steps.size() == 1);  // already on the upper floor
    CHECK(refroute::find(*ix, "tower", 1, 3, "cave", true).steps.size() == 2);  // the stairs first
    CHECK(refroute::find(*ix, "town", -1, -1, "town", true).found);             // here: no steps
    CHECK(refroute::find(*ix, "town", -1, -1, "town", true).steps.empty());
}

TEST(reference_route_a_condition_only_when_there_is_no_other_way) {
    const auto ix = index(false);  // no stairs: the tower's upper floor out of reach
    const refroute::Route r = refroute::find(*ix, "town", -1, -1, "cave", true);
    CHECK(r.found && r.conditional);
    CHECK_EQ(maps(r), std::string("cave"));
    CHECK_EQ(r.steps[0].move.npc, std::string("Guide"));
}

TEST(reference_route_kafra_and_nearest_warp) {
    const auto ix = index();
    CHECK(!refroute::find(*ix, "town", -1, -1, "isle", true).found);  // Eden's way: where it leads depends
    const refroute::Route k = refroute::find(*ix, "town", -1, -1, "isle", false);
    CHECK(k.found && k.kafra && k.paid);
    CHECK_EQ(k.zeny, 1200LL);
    CHECK_EQ(maps(k), std::string("isle"));
    // two warps from the town to the field: the one nearer the player
    CHECK_EQ(refroute::find(*ix, "town", 8, 8, "field", true).steps[0].move.x, 9);
    CHECK_EQ(refroute::find(*ix, "town", 2, 1, "field", true).steps[0].move.x, 1);
    for (const refindex::Move& m : ix->maps.at("town").moves)
        if (m.npc == "Eden") CHECK(m.depends && !m.conditional && !m.paid);
}

TEST(reference_pages_map_card_says_how_to_get_there) {
    const auto ix = index();
    const refpages::Page cave = refpages::map(*ix, Lang::Uk, "cave", ctx("town"));
    size_t at = size_t(-1);
    for (size_t i = 0; i < cave.rows.size(); ++i)
        if (cave.rows[i].left == "Як дістатися") at = i;
    CHECK(at != size_t(-1));
    CHECK_EQ(cave.rows[at].right, std::string("з town"));
    CHECK_EQ(cave.rows[at + 1].left, std::string("Пішки"));
    CHECK_EQ(cave.rows[at + 1].right, std::string("переходів: 4"));
    // 3.0.11-beta.29: the way on the world map (the concept's v19_route)
    CHECK(cave.rows[at + 2].link.go == refpages::Go::World && cave.rows[at + 2].link.map == "to:cave");
    // 3.0.11-beta.26: its steps a section of their own after the main blocks
    CHECK(cave.rows[at + 3].style == refpages::Style::Heading && cave.rows[at + 3].left == "Кроки · Пішки");
    CHECK(cave.rows[at + 3].fold == refpages::Route1 && cave.rows[at + 3].right == "переходів: 4");
    CHECK_EQ(cave.rows[at + 4].left, std::string("1. → field"));
    CHECK_EQ(cave.rows[at + 4].right, std::string("вихід 1, 1"));
    CHECK(cave.rows[at + 4].link.go == refpages::Go::Map && cave.rows[at + 4].link.map == "field");
    CHECK_EQ(cave.rows[at + 6].right, std::string("перехід 1, 8"));  // the stairs
    const refpages::Page isle = refpages::map(*ix, Lang::Uk, "isle", ctx("town"));
    bool kafra = false, walk = false;
    for (const refpages::Row& r : isle.rows) {
        if (r.left == "Через Kafra") kafra = r.right == "переходів: 1 · 1 200 z";
        if (r.left == "Пішки") walk = true;
        if (r.left == "1. → isle") CHECK_EQ(r.right, std::string("Kafra 5, 5 · 1 200 z"));
    }
    CHECK(kafra && !walk);
    const refpages::Page here = refpages::map(*ix, Lang::Uk, "town", ctx("town"));
    for (const refpages::Row& r : here.rows) CHECK(r.left != "Як дістатися");
    const refpages::Page nowhere = refpages::map(*ix, Lang::Uk, "town", ctx("cave"));  // the cave has no way out
    bool none = false;
    for (const refpages::Row& r : nowhere.rows) none = none || r.left.find("Звідси шляху не знайдено") == 0;
    CHECK(none);
}

// 3.0.11-beta.9: the way followed on the minimap (core/route_follow) and its line (core/minimap_proto)
TEST(route_follow_made_again_wherever_the_player_goes) {
    const auto ix = index();
    routefollow::Follow f;
    f.active = true;
    f.target = "cave";
    CHECK(routefollow::update(f, *ix, "town", 1, 1));
    CHECK_EQ(f.route.steps.size(), size_t(4));
    CHECK_EQ(routefollow::next(f, "town")->move.to, std::string("field"));
    CHECK(!routefollow::update(f, *ix, "town", 2, 2));  // the same part, near: the same way
    CHECK(routefollow::update(f, *ix, "field", 8, 8));   // a step on
    CHECK_EQ(routefollow::next(f, "field")->move.to, std::string("tower"));
    CHECK(routefollow::update(f, *ix, "tower", 1, 1));   // the lower floor: the stairs first
    CHECK_EQ(routefollow::next(f, "tower")->move.to, std::string("tower"));
    CHECK(routefollow::update(f, *ix, "tower", 8, 3));   // up the stairs: another part
    CHECK_EQ(f.route.steps.size(), size_t(1));
    CHECK(routefollow::update(f, *ix, "cave", 5, 5));    // there
    CHECK(f.route.found && f.route.steps.empty());
    CHECK(routefollow::next(f, "cave") == nullptr);
    CHECK(!routefollow::update(f, *ix, "cave", 20, 20));  // no cells known there: by the distance alone
    CHECK(routefollow::update(f, *ix, "cave", 61, 20));
    routefollow::Follow off;
    CHECK(!routefollow::update(off, *ix, "town", 1, 1));  // not followed: nothing made
}

TEST(minimap_goal_line_and_its_end) {
    // 3.0.11-beta.31: the way's line (its cells, core/walk_path) ends at the square's edge when its next move is past
    // it; nothing of it west of the player
    const minimap::Layout L = minimap::layout(1920, 1080, 1.0f, false, 0);
    minimap::Scene sc;
    sc.map = minimap::MapSize{400, 400};
    sc.player = minimap::Cell{200, 200};
    sc.path = {minimap::Cell{200, 200}, minimap::Cell{390, 200}};  // east, out of the view
    minimap::Scene bare = sc;
    bare.path.clear();
    panel::Picture with, without;
    minimap::paint(with, L, sc, minimap::Texts{});
    minimap::paint(without, L, bare, minimap::Texts{});
    const int row = int(L.map.h / 2);
    const auto px = [](const panel::Picture& p, int x, int y) { return p.px[size_t(y) * size_t(p.width) + size_t(x)]; };
    CHECK(px(with, int(L.map.w) - 4, row) != px(without, int(L.map.w) - 4, row));  // to the edge
    CHECK(px(with, int(L.map.w / 4), row) == px(without, int(L.map.w / 4), row));  // not behind the player
    CHECK(px(with, 4, int(L.map.h) + 4) == px(without, 4, int(L.map.h) + 4));      // nothing past the square
}

TEST(reference_pages_route_rows_follow_the_way) {
    const auto ix = index();
    refpages::Follow f;
    f.can = true;
    const refpages::Page p = refpages::map(*ix, Lang::Uk, "cave", ctx("town", f));
    const refpages::Row* walk = nullptr;
    for (const refpages::Row& r : p.rows)
        if (r.left == "Пішки") walk = &r;
    CHECK(walk != nullptr);
    CHECK_EQ(walk->right, std::string("переходів: 4 · вести ›"));
    CHECK(walk->link.go == refpages::Go::Follow && walk->link.id == 0 && walk->link.map == "cave");
    CHECK(!walk->gold);
    f.map = "cave";
    const refpages::Page q = refpages::map(*ix, Lang::Uk, "cave", ctx("town", f));
    for (const refpages::Row& r : q.rows)
        if (r.left == "Пішки") CHECK(r.gold && r.right == "переходів: 4 · веду · зупинити ›");
    const refpages::Page none = refpages::map(*ix, Lang::Uk, "cave", ctx("town"));  // the new minimap off: no link
    for (const refpages::Row& r : none.rows)
        if (r.left == "Пішки") CHECK(r.link.go == refpages::Go::None && r.right == "переходів: 4");
}

// 3.0.11-beta.10: the players now (core/reference_live) - the site's online.json kept as counts, in a map's card
TEST(reference_live_counts_from_the_site) {
    const char* site = R"({"online":104,"peak":183,"count":103,"byMap":[
        {"code":"mjolnir_01","name":"Mjolnir Field 1","players":[{"name":"A","class":"Monk","level":87}],"count":10},
        {"code":"prontera","name":"Prontera","players":[],"count":3},{"code":"","count":2},
        {"code":"x\"y","count":1}]})";
    reflive::Online o;
    CHECK(reflive::fromSite(site, 1000, o));
    CHECK_EQ(o.online, 104);
    CHECK_EQ(o.at, 1000LL);
    CHECK_EQ(o.maps.size(), size_t(3));
    CHECK_EQ(o.maps["mjolnir_01"], 10);
    const std::string file = reflive::write(o);
    CHECK(file.find("Monk") == std::string::npos && file.find("\"A\"") == std::string::npos);  // no names kept
    CHECK(file.find("\"xy\":1") != std::string::npos);  // a code with anything else: left out of it
    reflive::Online back;
    CHECK(reflive::read(file, back));
    CHECK_EQ(back.maps["prontera"], 3);
    CHECK_EQ(back.online, 104);
    CHECK(reflive::fresh(back, 1000 + 600));
    CHECK(!reflive::fresh(back, 1000 + 601));
    CHECK(!reflive::fromSite("{\"online\":1}", 1, o));
    CHECK(!reflive::read("{\"v\":2}", o));
    const auto ix = index();
    std::map<std::string, int> players{{"cave", 5}};
    refpages::Context c = ctx("town");
    c.players = &players;
    const refpages::Page cave = refpages::map(*ix, Lang::Uk, "cave", c);
    CHECK(cave.rows[1].style == refpages::Style::Portrait && cave.rows[1].left == "гравців зараз: 5");
}

// 3.0.11-beta.12: how far each map is from the player's (core/reference_route movesFrom) - on foot, or fewer paying
TEST(reference_route_how_far_each_map_is) {
    const auto ix = index();
    const auto far = refroute::movesFrom(*ix, "town");
    CHECK_EQ(far.at("field").moves, 1);
    CHECK(!far.at("field").paid);
    CHECK_EQ(far.at("cave").moves, 4);  // the Guide's way has a condition: not counted
    CHECK(!far.at("cave").paid);
    CHECK_EQ(far.at("isle").moves, 1);  // by Kafra alone
    CHECK(far.at("isle").paid && far.at("isle").kafra);
    CHECK_EQ(far.at("town").moves, 0);
    CHECK(refroute::movesFrom(*ix, "nowhere").empty());
    CHECK_EQ(refroute::movesFrom(*ix, "tower", 8, 3).at("cave").moves, 1);  // the upper floor
}

// 3.0.11-beta.29, the concept's v19_route: the world map's way to a map - the panel (the ways to choose, the warning,
// the steps, the buttons), the moves on the map (a teleport's arc and its price), the pin of a target off the sheet
TEST(reference_world_way) {
    const auto ix = index();
    worldmap::Sheet sh;
    CHECK(worldmap::parse(R"({"W":100,"H":60,"world":[{"id":"town","l":10,"t":10,"w":20,"h":20},
        {"id":"field","l":40,"t":10,"w":20,"h":20},{"id":"isle","l":70,"t":30,"w":20,"h":20}],"dungeon":[]})", sh));
    refpages::Follow follow;
    follow.can = true;
    refpages::Context c = ctx("town", follow);
    c.world = &sh;
    std::string code;
    int variant = 0;
    CHECK(refpages::worldTarget("to:prt_fild08:k", code, variant) && code == "prt_fild08" && variant == 1);
    CHECK(refpages::worldTarget("to:isle", code, variant) && code == "isle" && variant == -1);
    CHECK(!refpages::worldTarget("mob:1002", code, variant));
    const refpages::Page p = refpages::world(*ix, Lang::Uk, c, "to:tower");
    CHECK_EQ(p.name, std::string("Шлях"));
    size_t at = 0;
    while (at < p.rows.size() && p.rows[at].style != refpages::Style::Caption) ++at;
    CHECK(at < p.rows.size() && p.rows[at].left == "Шлях до tower" && p.rows[at + 1].left == "Від: town");
    // on foot the one way there (picked); its warning, its steps, what the marks mean, the buttons
    CHECK(p.rows[at + 2].left == "Пішки" && p.rows[at + 2].gold && p.rows[at + 2].link.map == "to:tower:w");
    CHECK_EQ(p.rows[at + 2].right, std::string("переходів: 2"));
    CHECK(p.rows[at + 3].style == refpages::Style::Warn);
    CHECK(p.rows[at + 4].style == refpages::Style::Step && p.rows[at + 4].right == "1");
    CHECK_EQ(p.rows[at + 4].left, std::string("field"));
    CHECK(p.rows[at + 5].left == "tower" && p.rows[at + 5].sub == "вихід 8, 8" && p.rows[at + 5].tone == 0);
    CHECK(p.rows.back().style == refpages::Style::Button && p.rows.back().link.go == refpages::Go::World);
    const refpages::Row& lead = p.rows[p.rows.size() - 2];
    CHECK(lead.style == refpages::Style::Button && lead.gold && lead.link.go == refpages::Go::Follow);
    CHECK(lead.link.id == 0 && lead.link.map == "tower");
    // to the isle Eden's officer leads elsewhere too (no way): by Kafra alone - its price, its step pink
    // by Kafra: a teleport - its arc's words; the step pink
    const refpages::Page k = refpages::world(*ix, Lang::Uk, c, "to:isle");
    size_t kat = 0;
    while (kat < k.rows.size() && k.rows[kat].style != refpages::Style::Caption) ++kat;
    CHECK(k.rows[kat + 2].left == "Через Kafra" && k.rows[kat + 2].gold);
    CHECK(k.rows[kat + 3].style == refpages::Style::Warn);
    CHECK_EQ(k.rows[kat + 2].right, std::string("переходів: 1 · 1 200 z"));
    const refpages::WorldWay kafra = refpages::worldWayOf(*ix, Lang::Uk, c, "to:isle:k");
    CHECK(kafra.hops.size() == 1 && kafra.hops[0].jump && kafra.hops[0].label == "Kafra · 1 200 z");
    CHECK(kafra.target == "isle" && kafra.targetLabel == "isle" && kafra.hereLabel == "Ви тут · town");
    bool pink = false;
    for (const refpages::Row& r : k.rows) pink = pink || (r.style == refpages::Style::Step && r.tone == 1);
    CHECK(pink);
    // the tower is off the sheet: its pin on the field (the last map of the way there), «… → підземелля»
    const refpages::WorldWay tower = refpages::worldWayOf(*ix, Lang::Uk, c, "to:tower");
    CHECK(tower.hops.size() == 2 && !tower.hops[0].jump && tower.hops[1].to == "tower");
    CHECK_EQ(tower.targetLabel, std::string("tower → підземелля"));
    float l = 0, t = 0, r = 0, b = 0;
    CHECK(refpages::worldFocus(sh, tower, {}, l, t, r, b) && l == 10 && t == 10 && r == 60 && b == 30);
    // a location's card leads to its way on the world map
    const refpages::Page card = refpages::map(*ix, Lang::Uk, "isle", c);
    CHECK(std::any_of(card.rows.begin(), card.rows.end(), [](const refpages::Row& row) {
        return row.link.go == refpages::Go::World && row.link.map == "to:isle";
    }));
    // the window: the gold way, a teleport's words, the pin's words - inside the map, none under the pointer's card
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    int total = 0;
    const auto rows = refwin::place(L, k, total);
    size_t map = 0;
    while (k.rows[map].style != refpages::Style::World) ++map;
    panel::Picture sheet;
    sheet.width = rows[map].w;
    sheet.height = rows[map].h;
    sheet.px.assign(size_t(sheet.width) * size_t(sheet.height), 0xFF181B57u);
    refwin::Look look;
    look.world.picture = &sheet;
    look.world.sheet = &sh;
    look.world.here = "town";
    look.world.hops = kafra.hops;
    look.world.target = kafra.target;
    look.world.targetLabel = "isle";
    look.world.hereLabel = kafra.hereLabel;
    look.world.hover = "field";
    look.world.tip = {"field", "field · видів монстрів: 2"};
    refwin::Texts texts;
    const std::vector<refwin::Words> words = refwin::words(L, k, rows, 0, texts, look);
    const panel::Box box{float(rows[map].x), float(L.bodyY + rows[map].y), float(rows[map].w), float(rows[map].h)};
    bool price = false, here = false, tip = false;
    for (const refwin::Words& w : words) {
        price = price || w.text == "Kafra · 1 200 z";
        here = here || w.text == "Ви тут · town";
        tip = tip || w.text == "field · видів монстрів: 2";
        if (w.text == "Kafra · 1 200 z" || w.text == "Ви тут · town")
            CHECK(w.box.x >= box.x && w.box.y >= box.y && w.box.x + w.box.w <= box.x + box.w);
    }
    CHECK(price && here && tip);
    panel::Picture pic;
    refwin::paint(pic, L, k, rows, 0, total, look);
    CHECK_EQ(pic.width, L.w);
}

// 3.0.11-beta.64 (the concept's v19_route: «Підземелля Glast Heim» - the dungeon's maps in a box at the world map's
// bottom left; «184 локації · 128 підземель» under it): a dungeon's way - its maps from its way in on the sheet to the
// target, their names short of the group's words; the counts on the world map's page
TEST(reference_world_dungeon_chain) {
    const auto ix = index();
    ix->mapNames["tower"] = "Old Tower Hall";  // the group's words cut off
    ix->mapNames["cave"] = "Old Tower";        // nothing left of it: its code
    worldmap::Sheet sh;
    CHECK(worldmap::parse(R"({"W":100,"H":60,"world":[{"id":"town","l":10,"t":10,"w":20,"h":20},
        {"id":"field","l":40,"t":10,"w":20,"h":20},{"id":"isle","l":70,"t":30,"w":20,"h":20}],
        "dungeon":[{"id":"tower","group":"Old Tower"},{"id":"cave","group":"Old Tower"}]})", sh));
    refpages::Context c = ctx("town");
    c.world = &sh;
    const refpages::WorldWay w = refpages::worldWayOf(*ix, Lang::Uk, c, "to:cave:w");
    CHECK_EQ(w.chain.size(), size_t(3));
    if (w.chain.size() != 3) return;
    CHECK(w.chain[0].first == "field" && w.chain[1].first == "tower" && w.chain[2].first == "cave");
    CHECK(w.chain[0].second == "field" && w.chain[1].second == "Hall" && w.chain[2].second == "cave");
    CHECK_EQ(w.chainTitle, std::string("Підземелля Old Tower"));
    CHECK(refpages::worldWayOf(*ix, Lang::Uk, c, "to:isle:k").chain.empty());  // a way on the sheet: none
    CHECK(refpages::worldWayOf(*ix, Lang::Uk, c, "to:field").chain.empty());
    // the counts under the world map's caption
    const refpages::Page home = refpages::world(*ix, Lang::Uk, c, "");
    CHECK(std::any_of(home.rows.begin(), home.rows.end(),
                      [](const refpages::Row& r) { return r.left == "3 локації · 2 підземелля"; }));
    // the window: the box at the map's bottom left - its title, each map's picture and name; a click: its card
    const refpages::Page p = refpages::world(*ix, Lang::Uk, c, "to:cave:w");
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    int total = 0;
    const auto rows = refwin::place(L, p, total);
    size_t map = 0;
    while (map < p.rows.size() && p.rows[map].style != refpages::Style::World) ++map;
    CHECK(map < p.rows.size());
    if (map >= p.rows.size()) return;
    panel::Picture sheet;
    sheet.width = rows[map].w;
    sheet.height = rows[map].h;
    sheet.px.assign(size_t(sheet.width) * size_t(sheet.height), 0xFF181B57u);
    const int side = refwin::chainSide(L);
    CHECK_EQ(side, 64);
    panel::Picture red;
    red.width = red.height = side;
    red.px.assign(size_t(side) * size_t(side), 0xFFC03030u);
    refwin::Look look;
    look.world.picture = &sheet;
    look.world.sheet = &sh;
    look.world.here = "town";
    look.world.hops = w.hops;
    look.world.target = w.target;
    look.world.targetLabel = w.targetLabel;
    look.world.chainTitle = w.chainTitle;
    for (const auto& [code, name] : w.chain)
        look.world.chain.push_back(refwin::ChainMap{code, name, code == "tower" ? &red : nullptr});
    const std::vector<refwin::Words> words = refwin::words(L, p, rows, 0, refwin::Texts{}, look);
    const panel::Box box{float(rows[map].x), float(L.bodyY + rows[map].y), float(rows[map].w), float(rows[map].h)};
    const auto find = [&](const std::string& t) {
        return std::find_if(words.begin(), words.end(), [&](const refwin::Words& x) { return x.text == t; });
    };
    CHECK(find("Підземелля Old Tower") != words.end() && find("field") != words.end());
    CHECK(find("Hall") != words.end() && find("cave") != words.end());
    for (const char* t : {"Підземелля Old Tower", "field", "Hall", "cave"}) {  // in the map's bottom left
        const auto it = find(t);
        if (it == words.end()) continue;
        CHECK(it->box.x >= box.x && it->box.x + it->box.w <= box.x + box.w / 2);
        CHECK(it->box.y > box.y + box.h / 2 && it->box.y + it->box.h <= box.y + box.h);
    }
    panel::Picture pic;
    refwin::paint(pic, L, p, rows, 0, total, look);
    const auto hall = find("Hall");
    if (hall == words.end()) return;
    // its picture over its name
    const float cx = hall->box.x + hall->box.w / 2, cy = hall->box.y - float(side) / 2 - 6;
    CHECK(pic.px[size_t(int(cy)) * size_t(pic.width) + size_t(int(cx))] == 0xFFC03030u);
    const refwin::Chrome ch = refwin::chrome(L, p, refwin::Texts{});
    const refwin::Under u = refwin::under(L, p, rows, 0, ch, cx, cy, &look.world);
    CHECK(u.part == refwin::Part::Chain && u.tile == 1);
}
