// 3.0.11-beta.11: the world map of «Довідник» - the site's sheet (core/world_map), its page and its row in the window
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "core/reference_pages.hpp"
#include "core/reference_world_paint.hpp"
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

// 3.0.11-beta.64: a map's minimap for the world map's dungeon box - side x side, the game's magenta the sea's colour
TEST(world_map_thumbnail) {
    std::vector<uint32_t> picture(40 * 20, 0xFF20A040);  // a green field 40 x 20, its left half the game's magenta
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x) picture[size_t(y * 40 + x)] = 0xFFFF00FF;
    const panel::Picture t = worldmap::thumbnail(picture, 40, 20, 16);
    CHECK(t.width == 16 && t.height == 16 && t.px.size() == 256u);
    CHECK_EQ(t.px[size_t(8 * 16 + 12)], 0xFF20A040u);       // the right half: the field, stretched to the square
    CHECK_EQ(t.px[size_t(8 * 16 + 3)], worldmap::kGround);  // the left: the sea shows
    CHECK(worldmap::thumbnail(picture, 40, 20, 0).width == 0 && worldmap::thumbnail({}, 0, 0, 16).width == 0);
}

// 3.0.11-beta.72 (the owner: «розовые полосы по краям»): the pinkish pixels round the game's magenta see-through too
TEST(world_map_pinkish_is_see_through) {
    worldmap::Sheet sh;
    CHECK(worldmap::parse(kSheet, sh));
    std::vector<uint32_t> sheet(size_t(sh.w) * size_t(sh.h), worldmap::kGround);
    std::vector<uint32_t> picture(40 * 40, 0xFF20A040);  // a green field, its top two rows a smudged magenta
    for (int i = 0; i < 80; ++i) picture[size_t(i)] = i % 2 ? 0xFFF010E8 : 0xFFFA14F0;
    worldmap::put(sheet, sh.w, sh.h, sh.world[0], picture, 40, 40);
    CHECK_EQ(sheet[size_t(10 * sh.w + 20)], worldmap::kGround);  // the sea shows, no pink
    CHECK_EQ(sheet[size_t(20 * sh.w + 20)], 0xFF20A040u);
    const panel::Picture t = worldmap::thumbnail(picture, 40, 40, 20);
    CHECK_EQ(t.px[size_t(0 * 20 + 5)], worldmap::kGround);
}

// 3.0.11-beta.29: the concept's zoom (v19_route) - a view's fit, zooming round the pointer, the view on a part, the
// sheet smooth when zoomed in
TEST(world_map_view_zoom_and_pan) {
    const worldmap::Fit whole = worldmap::view(100, 50, 200, 200, 1, -1, -1);  // as fit: s 2, centred down
    CHECK(whole.s == 2.0f && whole.x == 0.0f && whole.y == 50.0f);
    const worldmap::Fit z = worldmap::view(100, 50, 200, 100, 2, 25, 25);  // the box's middle on (25, 25)
    CHECK(z.s == 4.0f && z.x == 0.0f && z.y == -50.0f);
    CHECK_EQ(worldmap::view(100, 50, 200, 100, 2, 95, 25).x, -200.0f);  // pulled back: no sea past the right edge
    CHECK_EQ(worldmap::view(100, 50, 200, 200, 99, -1, -1).s, 2.0f * worldmap::kMostZoom);
    float zoom = 1, cx = -1, cy = -1;
    worldmap::zoomAt(100, 50, 200, 100, zoom, cx, cy, 60, 40, 2);  // (60, 40) is the sheet's (30, 20): kept there
    CHECK(zoom == 2.0f);
    const worldmap::Fit after = worldmap::view(100, 50, 200, 100, zoom, cx, cy);
    CHECK(std::abs((60 - after.x) / after.s - 30) < 0.01f && std::abs((40 - after.y) / after.s - 20) < 0.01f);
    worldmap::viewOn(100, 50, 200, 100, 10, 10, 20, 20, zoom, cx, cy);  // a part 10 x 10, a margin round it
    CHECK(std::abs(zoom - 100.0f / 26) < 0.01f && cx == 15.0f && cy == 15.0f);
    worldmap::viewOn(100, 50, 200, 100, 0, 0, 100, 50, zoom, cx, cy);  // all of it: no zoom
    CHECK_EQ(zoom, 1.0f);
    std::vector<uint32_t> two{0xFF000000, 0xFFFFFFFF};  // black and white, four times as big: between them grey
    panel::Picture out;
    worldmap::scaled(out, two, 2, 1, 8, 4, worldmap::Fit{4, 0, 0});
    CHECK(out.px[0] == 0xFF000000u && out.px[7] == 0xFFFFFFFFu);
    CHECK((out.px[3] & 0xFF) > 60 && (out.px[3] & 0xFF) < 130 && (out.px[4] & 0xFF) > (out.px[3] & 0xFF));
}

size_t worldRow(const refpages::Page& p) {
    for (size_t i = 0; i < p.rows.size(); ++i)
        if (p.rows[i].style == refpages::Style::World) return i;
    return size_t(-1);
}

TEST(reference_world_pages) {
    worldmap::Sheet sh;
    CHECK(worldmap::parse(kSheet, sh));
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"commit":"c","date":"d","items":{},"mobs":{},"spawns":{},"routes":[],
        "shops":[],"npcs":[["prontera",150,180,"Kafra Employee",113]],"pets":[],"craft":[],"flags":{}})", ix));
    ix.mapNames = refindex::mapNames("prt_fild08.rsw#Prontera Field #\nprontera.rsw#Prontera City, Capitol#");
    refpages::Context c;
    c.here = "prontera";
    const refpages::Page none = refpages::world(ix, Lang::Uk, c);
    CHECK_EQ(none.rows.size(), size_t(3));  // no sheet yet: its tabs, a note
    CHECK(none.rows[2].style == refpages::Style::Note);
    c.world = &sh;
    // 3.0.11-beta.29, the concept's v19_route: the bar (its tabs, the zoom), the map, the panel at its right
    const refpages::Page w = refpages::world(ix, Lang::Uk, c);
    CHECK(w.rows[0].style == refpages::Style::Tab && w.rows[0].gold && w.rows[0].link.go == refpages::Go::World);
    CHECK(w.rows[1].style == refpages::Style::Tab && w.rows[1].link.id == 1 && w.rows[1].icon == "ui:dungeon");
    CHECK(w.rows[2].style == refpages::Style::Tool && w.rows[2].link.go == refpages::Go::Zoom);
    CHECK(w.rows[2].link.id == -1 && w.rows[3].link.id == 0 && w.rows[4].link.id == 1);
    CHECK_EQ(worldRow(w), size_t(5));
    CHECK(w.rows[6].style == refpages::Style::Caption && w.rows[6].left == "Карта світу");
    CHECK(w.rows.back().style == refpages::Style::Legend);
    std::map<std::string, int> players{{"prt_fild08", 4}, {"prontera", 12}};
    c.players = &players;
    c.hover = "prt_fild08";  // the pointer's map no longer changes the page: its card is over the map
    const refpages::Page h = refpages::world(ix, Lang::Uk, c);
    CHECK(h.rows[2].style == refpages::Style::Tab && h.rows[2].left == "16 онлайн" && h.rows[2].dot != 0);
    const size_t top = size_t(std::find_if(h.rows.begin(), h.rows.end(), [](const refpages::Row& r) {
                                  return r.left == "Найбільше гравців зараз";
                              }) - h.rows.begin());
    CHECK(top < h.rows.size() && h.rows[top + 1].link.map == "prontera" && h.rows[top + 1].right == "12");
    const std::vector<std::string> tip = refpages::worldTip(ix, Lang::Uk, "prt_fild08", c);
    CHECK(tip.size() == 3 && tip[0] == "Prontera Field" && tip[1] == "prt_fild08" && tip[2] == "гравців зараз: 4");
    const std::vector<std::string> bare = refpages::worldTip(ix, Lang::Uk, "izlude", c);  // no name, no one there
    CHECK(bare.size() == 2 && bare[0] == "izlude" && bare[1].empty());
    const auto towns = refpages::worldLabels(ix);  // a Kafra and no monsters: a town
    CHECK(towns.size() == 1 && towns[0].first == "prontera");
    const refpages::Page d = refpages::dungeons(ix, Lang::Uk, c);
    CHECK(d.rows[0].style == refpages::Style::Tab && !d.rows[0].gold && d.rows[1].gold);
    CHECK(d.rows[3].style == refpages::Style::Heading && d.rows[3].left == "Prontera Culvert");
    CHECK(d.rows[4].link.go == refpages::Go::Map && d.rows[4].link.map == "prt_sewb1");
    const refpages::Page home = refpages::home(ix, Lang::Uk);
    CHECK(home.rows[11].link.go == refpages::Go::World && home.rows[11].icon == "ui:world");  // the tenth tile
    // the window's world map: the tile under the pointer at a zoom (its own pixels)
    for (const float k : {1.0f, 2.0f}) {
        refwin::Layout L;
        L.s = k;
        const panel::Box box{0, 0, 900 * k, 560 * k};
        refwin::WorldLook look;
        look.sheet = &sh;
        for (const float zoom : {1.0f, 2.0f}) {
            look.zoom = zoom;
            look.cx = 20;
            look.cy = 20;
            const worldmap::Fit f = worldmap::view(sh.w, sh.h, int(box.w), int(box.h), zoom, 20, 20);
            const refwin::Under u = refwin::worldUnder(L, box, look, f.x + 15 * f.s, f.y + 15 * f.s);
            CHECK(u.part == refwin::Part::Tile && u.tile == 0);
        }
    }
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
    const refpages::Row& caption = p.rows[worldRow(p) + 1];  // 3.0.11-beta.29: the panel's title, its maps under it
    CHECK(caption.style == refpages::Style::Caption && caption.left == "Poring на карті світу");
    CHECK(p.rows[worldRow(p) + 2].left == "prt_fild08" && p.rows[worldRow(p) + 2].right == "×100");
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

