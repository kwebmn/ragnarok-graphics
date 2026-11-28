#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "core/window_sizes.hpp"
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
    const char* tabs[iw::kTabs] = {"Усе", "Витратне", "Спорядж.", "Карти", "Різне", "Обране"};
    for (int i = 0; i < iw::kTabs; ++i) t.tabs[i] = tabs[i];
    t.search = "Пошук за назвою";
    t.weight = "Вага";
    t.use = "двічі клік — використати";
    t.info = "ПКМ — опис";
    t.fresh = "нове";
    t.keys = "ПКМ — опис · Alt+ПКМ — перенести цілим";  // 3.0.24-beta.20: the keys' hint under the grid
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

void make(Grid& g, int things, windowsizes::Cells cells = windowsizes::Cells{8, 5}) {
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
    g.L = iw::layout(0, 0, 1, texts(), measure, cells);
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
    CHECK(std::abs(float(g.L.w) - 536) < 2.5f);  // 3.0.24-beta.20: the tabs' column at the left (500 before)
    CHECK(g.L.cells[0].y > g.L.search.y + g.L.search.h);  // the grid under the search
    CHECK(g.L.footer.y >= g.L.cells[39].y + g.L.cells[39].h);
    CHECK_EQ(g.L.tabs.size(), size_t(iw::kTabs));
    // 3.0.24-beta.20 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md: «наши шесть вертикально
    // слева»): the tabs one under another, at the grid's left; the keys' hint between the grid and the footer
    for (size_t i = 1; i < g.L.tabs.size(); ++i) {
        CHECK(std::abs(g.L.tabs[i].x - g.L.tabs[0].x) < 0.5f);
        CHECK(g.L.tabs[i].y >= g.L.tabs[i - 1].y + g.L.tabs[i - 1].h - 0.5f);
    }
    CHECK(g.L.tabs[0].x + g.L.tabs[0].w <= g.L.cells[0].x);
    CHECK(g.L.tabs[0].y >= g.L.title.y + g.L.title.h - 0.5f);
    if (Rml::Element* keys = g.doc->QuerySelector(".keys")) {
        const float ky = keys->GetAbsoluteOffset(Rml::BoxArea::Border).y - 30;  // the document at (40, 30)
        CHECK(ky >= g.L.cells[39].y + g.L.cells[39].h - 0.5f && ky < g.L.footer.y);
        CHECK(keys->GetInnerRML().find("Alt+") != Rml::String::npos);
    } else {
        CHECK(false);  // no keys' hint
    }
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
    CHECK(std::abs(float(L.w) - 670) < 2.5f);  // 3.0.24-beta.20: 44.6667 rem of 15 (625 before the tabs' column)
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

// 3.0.24-beta.28 (the final review of part 3): the windows' shared drawn cross (.cxa/.cxb, centred by its margins)
// must not move «Речі»'s own - its bars in the middle of its close's box
TEST(uikit_item_grid_close_cross_centred) {
    Grid g;
    make(g, 3);
    Rml::ElementList bars, closes;
    if (g.doc) g.doc->GetElementsByClassName(bars, "cxa"), g.doc->GetElementsByClassName(closes, "x");
    CHECK(!bars.empty() && !closes.empty());
    if (bars.empty() || closes.empty()) return;
    const auto centre = [](Rml::Element* e) {
        const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border);
        const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
        return Rml::Vector2f{at.x + size.x / 2, at.y + size.y / 2};
    };
    const Rml::Vector2f bar = centre(bars[0]), box = centre(closes[0]);
    std::printf("  the cross's bar at %.1f, %.1f - its box's middle %.1f, %.1f\n", bar.x, bar.y, box.x, box.y);
    CHECK(std::abs(bar.x - box.x) < 1.0f && std::abs(bar.y - box.y) < 1.0f);
    rmlhost::close(g.doc);
}

// R-AVW884 (3.0.24-beta.33: the turned words in their tabs' middle). 3.0.24-beta.50 (the audit 28.11, item 6: the
// turned words hard to read): the tabs are pictures - each its own icon in the tab's middle, the tab no taller than
// it is wide and a half; the tab's name beside it, over the grid, only while the tab is pointed at
TEST(uikit_item_grid_tabs_are_icons_named_on_hover) {
    for (const float scale : {1.0f, 1.75f}) {
        iw::Texts t = texts();
        const char* tabs[iw::kTabs] = {"All", "Usable", "Gear", "Cards", "Misc", "Starred"};
        for (int i = 0; i < iw::kTabs; ++i) t.tabs[i] = tabs[i];
        std::vector<iw::Item> items(3);
        std::vector<int> shown{0, 1, 2};
        iw::Show sh;
        sh.tab = iw::Tab(3);
        sh.hoverTab = 1;
        iw::Layout L = iw::layout(0, 0, scale, t, measure);
        static ig::Model m;
        m.tabIcons.clear();
        for (int i = 0; i < iw::kTabs; ++i) m.tabIcons.push_back("pic:tab" + std::to_string(i));
        ig::fill(m, L, items, shown, sh, t, [](const panel::Picture*) { return std::string(); });
        std::string error;
        Rml::ElementDocument* d =
            rmlhost::load(ig::rml(), [&](Rml::DataModelConstructor& c) { ig::bind(c, m); }, 0, 0, &error);
        CHECK(d != nullptr);
        if (!d) continue;
        rmlhost::update();
        Rml::ElementList tabsFound;
        d->QuerySelectorAll(tabsFound, ".tab");
        int seen = 0, tips = 0;
        for (Rml::Element* tab : tabsFound) {
            if (!tab->IsVisible(true)) continue;
            const Rml::Vector2f ta = tab->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f ts = tab->GetBox().GetSize(Rml::BoxArea::Border);
            CHECK(tab->QuerySelector(".tl") == nullptr);  // no turned word
            CHECK(ts.y <= ts.x * 1.5f + 0.5f);
            if (Rml::Element* img = tab->QuerySelector("img")) {
                const Rml::Vector2f ia = img->GetAbsoluteOffset(Rml::BoxArea::Border);
                const Rml::Vector2f is = img->GetBox().GetSize(Rml::BoxArea::Border);
                CHECK(img->GetAttribute<Rml::String>("src", "") == "pic:tab" + std::to_string(seen));
                CHECK(std::abs(ta.x + ts.x / 2 - (ia.x + is.x / 2)) < 1.0f);
                CHECK(std::abs(ta.y + ts.y / 2 - (ia.y + is.y / 2)) < 1.0f);
            } else {
                CHECK(false);  // a tab without its icon
            }
            if (Rml::Element* tip = tab->QuerySelector(".tip")) {
                if (tip->IsVisible(true)) {
                    ++tips;
                    CHECK(seen == 1);
                    CHECK(tip->GetInnerRML() == "Usable");
                    CHECK(tip->GetAbsoluteOffset(Rml::BoxArea::Border).x >= ta.x + ts.x - 0.5f);  // beside it
                }
            }
            ++seen;
        }
        CHECK_EQ(seen, iw::kTabs);
        CHECK_EQ(tips, 1);
        rmlhost::close(d);
    }
}

// 3.0.24-beta.48 (the audit 28.11, the owner's «сделать»): its own columns and rows laid out by RmlUi too - ten a row,
// six rows, the cells as big as before, the window wider by the two columns
TEST(uikit_item_grid_lays_out_its_own_columns_and_rows) {
    Grid g;
    make(g, 70, windowsizes::Cells{10, 6});
    CHECK(g.doc != nullptr);
    CHECK(ig::refit(g.doc, g.L));
    CHECK_EQ(g.L.cells.size(), size_t(60));
    CHECK(std::abs(g.L.cells[9].y - g.L.cells[0].y) < 0.5f);       // ten a row
    CHECK(std::abs(g.L.cells[10].y - g.L.cells[0].y - 56) < 0.5f);
    CHECK(std::abs(g.L.cells[1].x - g.L.cells[0].x - 56) < 0.5f);  // the cells as big as before
    CHECK(std::abs(float(g.L.w) - (536 + 2 * 56)) < 2.5f);
    CHECK(g.m.scrollable);  // 70 things, 60 cells
    rmlhost::close(g.doc);
}
