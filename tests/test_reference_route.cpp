// 3.0.11-beta.8: the routes of «Довідник» - the maps' walkable parts (core/walk_grid) and the way over the pack's moves
// on them (core/reference_route), and «Як дістатися» in a map's card.
#include <cmath>
#include <memory>
#include <string>

#include "core/minimap_proto.hpp"
#include "core/reference_live.hpp"
#include "core/reference_pages.hpp"
#include "core/reference_route.hpp"
#include "core/route_follow.hpp"
#include "core/walk_grid.hpp"
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
    CHECK_EQ(cave.rows[at + 2].left, std::string("1. → field"));
    CHECK_EQ(cave.rows[at + 2].right, std::string("вихід 1, 1"));
    CHECK(cave.rows[at + 2].link.go == refpages::Go::Map && cave.rows[at + 2].link.map == "field");
    CHECK_EQ(cave.rows[at + 4].right, std::string("перехід 1, 8"));  // the stairs
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
    const minimap::Geometry g = minimap::geometry(1920, 1080, 1.0f);
    const minimap::MapSize map{400, 400};
    const minimap::Cell player{200, 200};
    float x = 0, y = 0;
    CHECK(minimap::goalPoint(g, map, player, 100, minimap::Cell{220, 200}, x, y));  // in the view: the cell itself
    CHECK(x > float(g.size) / 2 + 20 && x < float(g.size) && std::fabs(y - float(g.size) / 2) < 2);
    CHECK(!minimap::goalPoint(g, map, player, 100, minimap::Cell{390, 200}, x, y));  // east, out of it: the edge
    CHECK(x > float(g.size) - 8 && x < float(g.size) && std::fabs(y - float(g.size) / 2) < 2);
    CHECK(!minimap::goalPoint(g, map, player, 100, minimap::Cell{200, 10}, x, y));  // south: the bottom edge
    CHECK(y > float(g.size) - 8 && std::fabs(x - float(g.size) / 2) < 2);
    panel::Picture with, without;
    const std::vector<uint32_t> none;
    minimap::paint(without, g, none, 0, 0, map, player, 100);
    minimap::paint(with, g, none, 0, 0, map, player, 100, {}, minimap::Goal{true, minimap::Cell{230, 200}});
    const size_t mid = size_t(g.size / 2) * size_t(with.width) + size_t(g.size / 2 + g.size / 5);  // on the line
    CHECK(with.px[mid] != without.px[mid]);
    CHECK((with.px[mid] & 0xFF) > 0x80);  // the way's light blue over the dark ground (0x22 blue)
    const size_t off = size_t(g.size / 4) * size_t(with.width) + size_t(g.size / 4);  // away from it
    CHECK_EQ(with.px[off], without.px[off]);
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
    CHECK(cave.rows[1].style == refpages::Style::Sub && cave.rows[1].left == "гравців зараз: 5");
    const refpages::Page home = refpages::home(*ix, Lang::Uk, 104);
    CHECK_EQ(home.rows[home.rows.size() - 2].left, std::string("Зараз у грі: 104"));
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
