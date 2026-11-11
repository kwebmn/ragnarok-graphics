#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/item_grid_live.hpp"

using namespace uagfx;
namespace iw = uagfx::itemwin;
namespace ig = uagfx::uikit::itemgrid;

namespace {

iw::Texts texts() {
    iw::Texts t;
    t.title = "Речі";
    t.hotkey = "Alt+E";
    const char* tabs[iw::kTabs] = {"Усе", "Витратне", "Спорядження", "Карти", "Різне", "★"};
    for (int i = 0; i < iw::kTabs; ++i) t.tabs[i] = tabs[i];
    t.search = "Пошук за назвою";
    t.weight = "Вага";
    t.use = "двічі клік — використати";
    t.info = "ПКМ — опис";
    t.fresh = "нове";
    return t;
}

float measure(const std::string& s, float px, bool bold) { return float(s.size()) * px * (bold ? 0.6f : 0.55f); }

struct Grid {
    std::vector<iw::Item> items;
    std::vector<int> shown;
    iw::Show sh;
    iw::Layout L;
    ig::Model m;
    Rml::ElementDocument* doc = nullptr;
};

void make(Grid& g, int things) {
    for (int i = 0; i < things; ++i) {
        iw::Item it;
        it.index = i;
        it.id = 500 + i;
        it.count = i + 1;
        it.name = "Thing " + std::to_string(i);
        it.refine = i == 3 ? 7 : 0;
        it.fresh = i == 2;
        it.starred = i == 4;
        g.items.push_back(it);
        g.shown.push_back(i);
    }
    g.sh.weight = 120;
    g.sh.weightMost = 800;
    g.sh.zeny = 15000;
    g.L = iw::layout(0, 0, 1, texts(), measure);
    ig::fill(g.m, g.L, g.items, g.shown, g.sh, texts(), [](const panel::Picture*) { return std::string(); });
    std::string error;
    g.doc = rmlhost::load(ig::rml(), [&](Rml::DataModelConstructor& c) { ig::bind(c, g.m); }, 40, 30, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
}

panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }

}  // namespace

// 3.0.18-beta.94: «Речі» laid out by RmlUi; its boxes put into the window's layout - the core hit test then finds
// the cells, tabs, search and × where they are drawn (the drop protocol of the live windows)
TEST(uikit_item_grid_refit_puts_the_drawn_cells_into_the_hit_test) {
    Grid g;
    make(g, 12);
    CHECK(g.doc != nullptr);
    CHECK(ig::refit(g.doc, g.L));
    CHECK_EQ(g.L.cells.size(), size_t(40));
    CHECK(std::abs(g.L.cells[1].x - g.L.cells[0].x - 56) < 0.5f);  // 48 a cell, 8 between
    CHECK(std::abs(g.L.cells[8].y - g.L.cells[0].y - 56) < 0.5f);  // eight a row
    CHECK(std::abs(g.L.cells[8].x - g.L.cells[0].x) < 0.5f);
    CHECK(std::abs(float(g.L.w) - 500) < 2.5f);
    CHECK(g.L.cells[0].y > g.L.search.y + g.L.search.h);  // the grid under the search
    CHECK(g.L.footer.y >= g.L.cells[39].y + g.L.cells[39].h);
    CHECK_EQ(g.L.tabs.size(), size_t(iw::kTabs));
    for (size_t i = 1; i < g.L.tabs.size(); ++i) CHECK(g.L.tabs[i].x > g.L.tabs[i - 1].x + g.L.tabs[i - 1].w);
    panel::Box p = mid(g.L.cells[9]);
    iw::Hit h = iw::hitTest(g.L, g.shown.size(), 0, p.x, p.y);
    CHECK(h.part == iw::Part::Cell);
    CHECK_EQ(h.index, 9);
    p = mid(g.L.cells[20]);  // past the things: the grid's empty place
    CHECK(iw::hitTest(g.L, g.shown.size(), 0, p.x, p.y).part == iw::Part::Grid);
    p = mid(g.L.tabs[2]);
    h = iw::hitTest(g.L, g.shown.size(), 0, p.x, p.y);
    CHECK(h.part == iw::Part::Tab);
    CHECK_EQ(h.index, 2);
    p = mid(g.L.search);
    CHECK(iw::hitTest(g.L, g.shown.size(), 0, p.x, p.y).part == iw::Part::Search);
    p = mid(g.L.close);
    CHECK(iw::hitTest(g.L, g.shown.size(), 0, p.x, p.y).part == iw::Part::Close);
    CHECK(iw::hitTest(g.L, g.shown.size(), 0, 30, 20).part == iw::Part::Title);
    CHECK(g.L.bar.w > 100);
    CHECK(g.L.bar.x < 150);  // the weight's bar right after «Вага» (not spread along the footer)
    rmlhost::close(g.doc);
}

// The scroll counted: a row down, the first cell is the ninth thing; the star's corner its own part
TEST(uikit_item_grid_scrolled_cells_and_the_star_corner) {
    Grid g;
    make(g, 50);
    g.sh.scroll = 1;
    ig::fill(g.m, g.L, g.items, g.shown, g.sh, texts(), [](const panel::Picture*) { return std::string(); });
    rmlhost::update();
    CHECK(ig::refit(g.doc, g.L));
    CHECK(g.m.scrollable);
    panel::Box p = mid(g.L.cells[0]);
    iw::Hit h = iw::hitTest(g.L, g.shown.size(), 1, p.x, p.y);
    CHECK(h.part == iw::Part::Cell);
    CHECK_EQ(h.index, 8);
    const panel::Box& c = g.L.cells[0];
    h = iw::hitTest(g.L, g.shown.size(), 1, c.x + 6, c.y + c.h - 6);
    CHECK(h.part == iw::Part::Star);
    rmlhost::close(g.doc);
}

// The tip beside the pointed cell, inside the window: over it, under it on the first row
TEST(uikit_item_grid_tip_beside_the_pointed_cell) {
    Grid g;
    make(g, 12);
    CHECK(ig::refit(g.doc, g.L));
    g.sh.hover = 1;
    ig::fill(g.m, g.L, g.items, g.shown, g.sh, texts(), [](const panel::Picture*) { return std::string(); });
    ig::placeTip(g.doc, g.m, g.L, 1);
    rmlhost::update();
    ig::placeTip(g.doc, g.m, g.L, 1);  // its own size known now
    rmlhost::update();
    CHECK(g.m.tip);
    CHECK_EQ(g.m.tip1, std::string("Thing 1 \xC3\x97") + "2");
    CHECK(g.m.tipY >= g.L.cells[1].y + g.L.cells[1].h);  // the first row: under the cell
    CHECK(g.m.tipX >= 0);
    rmlhost::close(g.doc);
}

// 3.0.18-beta.100: the window's own scale (the text size and «Речі»'s size settings) - its sizes in rem of a base
// the model gives: at 1.25 the cells 60 px every 70
TEST(uikit_item_grid_takes_the_windows_own_scale) {
    std::vector<iw::Item> items(3);
    std::vector<int> shown{0, 1, 2};
    iw::Show sh;
    iw::Layout L = iw::layout(0, 0, 1.25f, texts(), measure);
    static ig::Model m;
    ig::fill(m, L, items, shown, sh, texts(), [](const panel::Picture*) { return std::string(); });
    CHECK(std::abs(m.base - 15) < 0.01f);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(ig::rml(), [&](Rml::DataModelConstructor& c) { ig::bind(c, m); }, 0, 0, &error);
    CHECK_EQ(error, std::string());
    CHECK(ig::refit(d, L));
    CHECK(std::abs(L.cells[0].w - 60) < 0.5f);
    CHECK(std::abs(L.cells[1].x - L.cells[0].x - 70) < 0.5f);
    CHECK(std::abs(float(L.w) - 625) < 2.5f);
    rmlhost::close(d);
}

// 3.0.18-beta.175 (R-6PYQQG: «Речі» said all its data again ~60 times a second while open - each use's 0.6 s flash
// gave the window another generation, and the whole model went to RmlUi): the model's variables that changed alone
TEST(uikit_item_grid_tells_the_variables_that_changed_alone) {
    ig::Model was;
    was.weightText = "100 / 2000 · 5 %";
    was.fill = 5;
    was.cells.assign(4, ig::CellV{});
    was.tabs.assign(2, ig::TabV{});
    ig::Model now = was;
    CHECK(ig::changed(was, now).empty());  // the same: nothing to say
    now.cells[1].flash = 0.5f;               // a use's flash in its cell
    std::vector<std::string> names;
    for (const char* n : ig::changed(was, now)) names.push_back(n);
    CHECK(names.size() == 1 && names[0] == "cells");
    now = was;
    now.weightText = "120 / 2000 · 6 %";
    now.fill = 6;
    names.clear();
    for (const char* n : ig::changed(was, now)) names.push_back(n);
    CHECK(names.size() == 2);
    CHECK(std::find(names.begin(), names.end(), "weightText") != names.end());
    CHECK(std::find(names.begin(), names.end(), "fill") != names.end());
    now = was;
    now.tabs[0].hot = true;  // a tab pointed at
    names.clear();
    for (const char* n : ig::changed(was, now)) names.push_back(n);
    CHECK(names.size() == 1 && names[0] == "tabs");
}
