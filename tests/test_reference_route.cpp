// 3.0.11-beta.8: the routes of «Довідник» - the maps' walkable parts (core/walk_grid) and the way over the pack's moves
// on them (core/reference_route), and «Як дістатися» in a map's card.
#include <memory>
#include <string>

#include "core/reference_pages.hpp"
#include "core/reference_route.hpp"
#include "core/walk_grid.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

namespace {

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
    const refpages::Page cave = refpages::map(*ix, Lang::Uk, "cave", "town");
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
    const refpages::Page isle = refpages::map(*ix, Lang::Uk, "isle", "town");
    bool kafra = false, walk = false;
    for (const refpages::Row& r : isle.rows) {
        if (r.left == "Через Kafra") kafra = r.right == "переходів: 1 · 1 200 z";
        if (r.left == "Пішки") walk = true;
        if (r.left == "1. → isle") CHECK_EQ(r.right, std::string("Kafra 5, 5 · 1 200 z"));
    }
    CHECK(kafra && !walk);
    const refpages::Page here = refpages::map(*ix, Lang::Uk, "town", "town");
    for (const refpages::Row& r : here.rows) CHECK(r.left != "Як дістатися");
    const refpages::Page nowhere = refpages::map(*ix, Lang::Uk, "town", "cave");  // the cave has no way out
    bool none = false;
    for (const refpages::Row& r : nowhere.rows) none = none || r.left.find("Звідси шляху не знайдено") == 0;
    CHECK(none);
}
