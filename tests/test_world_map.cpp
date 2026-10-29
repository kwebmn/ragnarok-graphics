// 3.0.11-beta.11: the world map of «Довідник» - the site's sheet (core/world_map), its page and its row in the window
#include <map>
#include <string>
#include <vector>

#include "core/reference_pages.hpp"
#include "core/reference_window.hpp"
#include "core/world_map.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

namespace {

const char* kSheet = R"({"W":100,"H":50,
"world":[{"id":"prontera","l":10,"t":10,"w":20,"h":20},{"id":"prt_fild08","l":10,"t":30,"w":20,"h":20},
         {"id":"izlude","l":40,"t":10,"w":10,"h":10},{"id":"bad","l":0,"t":0,"w":0,"h":5}],
"dungeon":[{"id":"prt_sewb1","group":"Prontera Culvert"},{"id":"prt_sewb2","group":"Prontera Culvert"},
           {"id":"pay_dun00","group":"Payon Cave"}]})";

}  // namespace

TEST(world_map_sheet_tiles_and_fit) {
    worldmap::Sheet sh;
    CHECK(worldmap::parse(kSheet, sh));
    CHECK_EQ(sh.w, 100);
    CHECK_EQ(sh.world.size(), size_t(3));  // a tile without a size left out
    CHECK_EQ(sh.dungeons.size(), size_t(3));
    CHECK_EQ(sh.dungeons[2].group, std::string("Payon Cave"));
    CHECK(!worldmap::parse("{\"W\":0,\"H\":5,\"world\":[]}", sh));
    CHECK(worldmap::parse(kSheet, sh));
    const worldmap::Fit f = worldmap::fit(sh.w, sh.h, 200, 200);  // twice as big, centred down the height
    CHECK(f.s == 2.0f && f.x == 0.0f && f.y == 50.0f);
    CHECK_EQ(worldmap::tileAt(sh, f, 2 * 15, 50 + 2 * 15), 0);   // prontera
    CHECK_EQ(worldmap::tileAt(sh, f, 2 * 45, 50 + 2 * 12), 2);   // izlude
    CHECK_EQ(worldmap::tileAt(sh, f, 2 * 80, 50 + 2 * 40), -1);  // the sea
    CHECK(worldmap::tileOf(sh, "prt_fild08") == &sh.world[1]);
    float x = 0, y = 0;
    CHECK(worldmap::cellOnSheet(sh.world[0], 400, 400, 200, 399, x, y));  // north up: y 399 is the tile's top row
    CHECK(x > 19.9f && x < 20.1f && y >= 10.0f && y < 10.1f);
    CHECK(!worldmap::cellOnSheet(sh.world[0], 400, 400, 400, 0, x, y));
}

TEST(world_map_tiles_put_on_the_sheet) {
    worldmap::Sheet sh;
    CHECK(worldmap::parse(kSheet, sh));
    std::vector<uint32_t> sheet(size_t(sh.w) * size_t(sh.h), worldmap::kGround);
    std::vector<uint32_t> picture(40 * 40, 0xFF20A040);  // a green field, two rows of the game's magenta on top
    for (int i = 0; i < 80; ++i) picture[size_t(i)] = 0xFFFF00FF;
    worldmap::put(sheet, sh.w, sh.h, sh.world[0], picture, 40, 40);
    CHECK_EQ(sheet[size_t(20 * sh.w + 20)], 0xFF20A040u);  // inside
    CHECK_EQ(sheet[size_t(10 * sh.w + 20)], worldmap::kGround);  // its top row: magenta, the sea shows
    CHECK_EQ(sheet[size_t(5 * sh.w + 5)], worldmap::kGround);    // outside it
    panel::Picture out;
    worldmap::scaled(out, sheet, sh.w, sh.h, 50, 50, worldmap::fit(sh.w, sh.h, 50, 50));  // half: averaged
    CHECK_EQ(out.width, 50);
    CHECK_EQ(out.px[size_t((12 + 10) * 50 + 10)], 0xFF20A040u);
}

TEST(reference_world_pages) {
    worldmap::Sheet sh;
    CHECK(worldmap::parse(kSheet, sh));
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"commit":"c","date":"d","items":{},"mobs":{},"spawns":{},"routes":[],
        "shops":[],"npcs":[],"pets":[],"craft":[],"flags":{}})", ix));
    refpages::Context c;
    c.here = "prontera";
    const refpages::Page none = refpages::world(ix, Lang::Uk, c);
    CHECK_EQ(none.rows.size(), size_t(2));  // no sheet yet: a note
    CHECK(none.rows[1].style == refpages::Style::Note);
    c.world = &sh;
    const refpages::Page w = refpages::world(ix, Lang::Uk, c);
    CHECK(w.rows[0].style == refpages::Style::Title && w.rows[0].link.go == refpages::Go::World);
    CHECK_EQ(w.rows[0].link.id, 1);  // its «Підземелля ›»
    CHECK(w.rows[1].style == refpages::Style::World);
    CHECK(w.rows[2].style == refpages::Style::Note);
    std::map<std::string, int> players{{"prt_fild08", 4}};
    c.players = &players;
    c.hover = "prt_fild08";
    const refpages::Page h = refpages::world(ix, Lang::Uk, c);
    CHECK_EQ(h.rows[2].left, std::string("prt_fild08 · гравців зараз: 4"));
    const refpages::Page d = refpages::dungeons(ix, Lang::Uk, c);
    CHECK(d.rows[1].style == refpages::Style::Heading && d.rows[1].left == "Prontera Culvert");
    CHECK(d.rows[2].link.go == refpages::Go::Map && d.rows[2].link.map == "prt_sewb1");
    CHECK(d.rows[4].style == refpages::Style::Heading && d.rows[4].left == "Payon Cave");
    const refpages::Page home = refpages::home(ix, Lang::Uk);
    CHECK(home.rows[10].link.go == refpages::Go::World && home.rows[10].icon == "ui:world");  // the ninth tile
    // the window: the map's row takes the body's rest; the tile under the pointer
    const refwin::Layout L = refwin::layout(1920, 1080, 1.0f);
    int total = 0;
    const auto rows = refwin::place(L, w, total);
    CHECK(rows[1].h > 500 && total <= L.bodyH);
    const worldmap::Fit f = refwin::worldFit(sh, rows[1]);
    const float x = float(rows[1].x) + f.x + 15 * f.s, y = float(L.bodyY + rows[1].y) + f.y + 15 * f.s;
    const refwin::Under u = refwin::under(L, w, rows, 0, refwin::Chrome{}, x, y, &sh);
    CHECK(u.part == refwin::Part::Tile && u.row == 1 && u.tile == 0);
}

// 3.0.11-beta.19: a monster's maps on the world map - its numbers there, the ones off the sheet listed under it
TEST(reference_world_marks_a_monster) {
    worldmap::Sheet sh;
    CHECK(worldmap::parse(kSheet, sh));
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"commit":"c","date":"d","items":{},
        "mobs":{"1002":["Poring",1,50,2,1,1,3,1,1,0,[],[],null]},
        "spawns":{"prt_fild08":[[1002,100,0,0,0,0,0]],"prt_sewb1":[[1002,5,0,0,0,0,0]]},"routes":[],"shops":[],
        "npcs":[],"pets":[],"craft":[],"flags":{}})", ix));
    const auto marks = refpages::worldMarks(ix, "mob:1002");
    CHECK_EQ(marks.size(), size_t(2));
    CHECK_EQ(marks.at("prt_fild08"), 100);
    CHECK(refpages::worldMarks(ix, "").empty());
    refpages::Context c;
    c.world = &sh;
    const refpages::Page p = refpages::world(ix, Lang::Uk, c, "mob:1002");
    CHECK_EQ(p.rows[0].left, std::string("Poring на карті світу"));
    size_t inside = size_t(-1);
    for (size_t i = 0; i < p.rows.size(); ++i)
        if (p.rows[i].left == "Підземелля й приміщення") inside = i;
    CHECK(inside != size_t(-1));
    CHECK(p.rows[inside + 1].left == "prt_sewb1" && p.rows[inside + 1].right == "×5");  // not on the sheet
    const refpages::Page card = refpages::mob(ix, Lang::Uk, 1002, c);
    bool link = false;
    for (const refpages::Row& r : card.rows)
        link = link || (r.link.go == refpages::Go::World && r.link.map == "mob:1002");
    CHECK(link);
}

// 3.0.11-beta.20: an item on the world map - where to farm it (its droppers' maps)
TEST(reference_world_marks_an_items_droppers) {
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"commit":"c","date":"d","items":{"909":["Jellopy","Jellopy",3,6,1,0,0]},
        "mobs":{"1002":["Poring",1,50,2,1,1,3,1,1,0,[[909,7000]],[],null],"1113":["Drops",2,55,4,3,1,3,3,1,0,
        [[909,7000]],[],null]},"spawns":{"prt_fild08":[[1002,100,0,0,0,0,0],[1113,10,0,0,0,0,0]],
        "prt_fild05":[[1113,40,0,0,0,0,0]]},"routes":[],"shops":[],"npcs":[],"pets":[],"craft":[],"flags":{}})", ix));
    const auto marks = refpages::worldMarks(ix, "item:909");
    CHECK_EQ(marks.at("prt_fild08"), 110);  // Poring and Drops there
    CHECK_EQ(marks.at("prt_fild05"), 40);
    const refpages::Page card = refpages::item(ix, Lang::Uk, 909, refpages::Context{});
    bool link = false;
    for (const refpages::Row& r : card.rows)
        link = link || (r.link.go == refpages::Go::World && r.link.map == "item:909");
    CHECK(link);
}

// 3.0.11-beta.25 (R-77TCM7:
// «когда по меню вожу, лагат курсор и фпс падает»): another map under the pointer
// on the world map - its page made again, only the rows that changed and the two maps' frames painted again: the
// picture as painted whole
TEST(reference_world_hover_repaint_matches_the_whole) {
    worldmap::Sheet sh;
    CHECK(worldmap::parse(kSheet, sh));
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"commit":"c","date":"d","items":{},"mobs":{},"spawns":{},"routes":[],
        "shops":[],"npcs":[],"pets":[],"craft":[],"flags":{}})", ix));
    std::map<std::string, int> players{{"prt_fild08", 4}, {"prontera", 12}};
    refpages::Context c;
    c.here = "prontera";
    c.world = &sh;
    c.players = &players;
    int steps = 0, differing = 0, moved = 0;
    for (const float k : {1.0f, 2.0f}) {
        const refwin::Layout L = refwin::layout(int(1920 * k), int(1080 * k), k);
        c.hover.clear();
        refpages::Page was = refpages::world(ix, Lang::Uk, c);
        int total = 0;
        const std::vector<refwin::Placed> rows = refwin::place(L, was, total);
        panel::Picture sheet;  // the tiles fitted into the row: a pattern
        sheet.width = rows[1].w;
        sheet.height = rows[1].h;
        sheet.px.resize(size_t(sheet.width) * size_t(sheet.height));
        for (size_t i = 0; i < sheet.px.size(); ++i) sheet.px[i] = 0xFF000000u | uint32_t(i * 2654435761u >> 8);
        refwin::Look look;
        refwin::Texts texts;
        texts.back = "Назад до Довідник";
        look.chrome = refwin::chrome(L, was, texts);
        look.world.picture = &sheet;
        look.world.sheet = &sh;
        look.world.here = "prontera";
        look.world.players = &players;
        look.world.hereX = 12;
        look.world.hereY = 15;
        panel::Picture partial;
        refwin::paint(partial, L, was, rows, 0, total, look);
        for (const char* map : {"prt_fild08", "izlude", "prontera", "", "prt_fild08"}) {
            c.hover = map;
            const refpages::Page now = refpages::world(ix, Lang::Uk, c);
            int nowTotal = 0;
            const std::vector<refwin::Placed> nowRows = refwin::place(L, now, nowTotal);
            bool same = nowRows.size() == rows.size() && nowTotal == total;
            for (size_t i = 0; same && i < rows.size(); ++i)
                same = rows[i].x == nowRows[i].x && rows[i].y == nowRows[i].y && rows[i].h == nowRows[i].h;
            if (!same) ++moved;
            refwin::Look nowLook = look;
            nowLook.world.hover = map;
            std::vector<Rect> areas;
            for (size_t i = 0; i < now.rows.size() && i < was.rows.size(); ++i)
                if (!refwin::sameRow(now.rows[i], was.rows[i]))
                    areas.push_back(refwin::rowArea(L, now, nowRows, 0, int(i)));
            areas.push_back(refwin::worldArea(L, nowRows, 0, 1, sh, look.world.hover));
            areas.push_back(refwin::worldArea(L, nowRows, 0, 1, sh, map));
            for (const Rect& a : areas)
                if (!a.empty()) refwin::repaint(partial, L, now, nowRows, 0, nowTotal, nowLook, a);
            panel::Picture whole;
            refwin::paint(whole, L, now, nowRows, 0, nowTotal, nowLook);
            ++steps;
            if (partial.px != whole.px) ++differing;
            was = now;
            look = nowLook;
        }
    }
    CHECK_EQ(steps, 10);
    CHECK_EQ(moved, 0);  // the pointer's map told in the same row: the places kept
    CHECK_EQ(differing, 0);
    CHECK(!refwin::sameRow(refpages::Row{}, refpages::world(ix, Lang::Uk, c).rows[0]));
    refpages::Row a, b;
    b.icon = "item:501";
    CHECK(refwin::sameRow(a, a) && !refwin::sameRow(a, b));
    CHECK(refwin::worldArea(refwin::layout(1920, 1080, 1.0f), {}, 0, 1, sh, "prontera").empty());
}
