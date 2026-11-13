// 3.0.18-beta.239 (docs/superpowers/plans/2026-11-13-reference-rewrite.md): «Довідник» a live window - its chrome and
// the page's markup (uikit/reference_live) loaded by RmlUi on the host: no warnings, the rows' links, two columns
// with the next block into the shorter one, a long list across in columns, one column narrow, the world map's page.
#include <cmath>
#include <cstdio>
#include <string>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/reference_live.hpp"

using namespace uagfx;
namespace ur = uagfx::uikit::reference;
using refpages::Row;
using refpages::Style;

namespace {

Row row(Style s, const std::string& left, refpages::Go go = refpages::Go::None, int id = 0) {
    Row r;
    r.style = s;
    r.left = left;
    r.link.go = go;
    r.link.id = id;
    return r;
}

struct Loaded {
    Rml::ElementDocument* doc = nullptr;
    std::string warnings;  // RmlUi's since the load began
};

Loaded load(ur::Model& m) {
    const size_t before = rmlhost::log().size();
    Loaded l;
    std::string error;
    l.doc = rmlhost::load(ur::rml(), [&](Rml::DataModelConstructor& c) { ur::bind(c, m); }, 0, 0, &error);
    rmlhost::update();
    l.warnings = error + rmlhost::log().substr(before);
    // the context's one type register: the rail's row declared again by the next load of the same window (two lines)
    std::string kept;
    for (size_t at = 0; at < l.warnings.size();) {
        size_t end = l.warnings.find('\n', at);
        if (end == std::string::npos) end = l.warnings.size() - 1;
        const std::string line = l.warnings.substr(at, end + 1 - at);
        if (line.find("already declared") == std::string::npos && line.rfind("T: ", 0) != 0 &&
            line.find("DataModelHandle.h:") == std::string::npos)
            kept += line;
        at = end + 1;
    }
    l.warnings = kept;
    if (!l.warnings.empty()) std::printf("  RmlUi: %s", l.warnings.c_str());
    return l;
}

size_t count(Rml::Element* root, const char* selector) {  // those shown (not a data-for's template)
    Rml::ElementList found;
    if (root) root->QuerySelectorAll(found, selector);
    size_t n = 0;
    for (Rml::Element* e : found)
        if (!e->HasAttribute("data-for") && e->IsVisible(true)) ++n;
    return n;
}

bool within(Rml::Element* e, const char* cls) {
    for (; e; e = e->GetParentNode())
        if (e->IsClassSet(cls)) return true;
    return false;
}

refpages::Page card() {  // a card: its title and picture, three sections - two short, one folded
    refpages::Page p;
    p.rows.push_back(row(Style::Title, "Poring"));
    Row portrait = row(Style::Portrait, "Lv 1 · HP 50");
    portrait.picture = "mob:1002";
    portrait.tags = {"el1:Water 1", "gold:бос", "Plant"};
    p.rows.push_back(portrait);
    p.rows.push_back(row(Style::Heading, "Що випадає"));        // 2
    for (int i = 0; i < 4; ++i) p.rows.push_back(row(Style::Row, "Jellopy", refpages::Go::Item, 909));  // 3..6
    p.rows.push_back(row(Style::Heading, "Де знайти"));          // 7
    for (int i = 0; i < 2; ++i) p.rows.push_back(row(Style::Row, "prt_fild08", refpages::Go::Map));  // 8, 9
    Row fold = row(Style::Heading, "Шкода стихіями", refpages::Go::Fold, refpages::Elements);  // 10
    fold.fold = refpages::Elements;
    fold.closed = true;
    p.rows.push_back(fold);
    return p;
}

}  // namespace

TEST(uikit_reference_chrome_loads_bound) {
    ur::Model m;
    m.title = "Довідник";
    m.back = "Назад до Poring";
    m.find = "Пошук у довіднику";
    m.star = "★";
    m.starred = true;
    m.home = true;
    m.rail = {ur::RailV{"Головна", "", "", false}, ur::RailV{"Монстри", "", "1004", true}};
    m.crumbs = {"Монстри", "Poring"};
    m.pack = "дані 8.11";
    m.live = "14 у грі";
    m.live_state = 1;
    m.keys = "Alt+← назад";
    const Loaded l = load(m);
    CHECK(l.doc != nullptr);
    CHECK_EQ(l.warnings, std::string());
    CHECK(l.doc && l.doc->GetElementById("rq") && l.doc->GetElementById("body") && l.doc->GetElementById("scroll"));
    CHECK_EQ(count(l.doc, ".rri"), size_t(2));
    CHECK_EQ(count(l.doc, ".rcb"), size_t(2));
    CHECK(l.doc && l.doc->GetElementById("ring"));
    rmlhost::close(l.doc);
}

TEST(uikit_reference_card_in_two_columns) {
    const refpages::Page p = card();
    ur::Look look;
    look.picture = [](const std::string& key) { return key == "mob:1002" ? std::string("pic:1:1") : std::string(); };
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    Rml::Element* body = l.doc ? l.doc->GetElementById("body") : nullptr;
    CHECK(body != nullptr);
    CHECK_EQ(count(body, "[data-ln]"), size_t(7));  // six rows that lead somewhere and the folded heading
    CHECK_EQ(count(body, ".rcols"), size_t(1));
    CHECK_EQ(count(body, ".rcol"), size_t(2));
    CHECK_EQ(count(body, "img.rpimg"), size_t(1));
    CHECK_EQ(count(body, ".rtag"), size_t(3));
    // «Що випадає» (6 high) in the first column, «Де знайти» (4) in the second, the folded one under it (shorter)
    Rml::Element* cols[2] = {nullptr, nullptr};
    if (body) {
        Rml::ElementList c;
        body->QuerySelectorAll(c, ".rcol");
        if (c.size() == 2) cols[0] = c[0], cols[1] = c[1];
    }
    CHECK(cols[0] && cols[0]->QuerySelector("#r2") && cols[0]->QuerySelector("#r6"));
    CHECK(cols[1] && cols[1]->QuerySelector("#r7") && cols[1]->QuerySelector("#r10"));
    Rml::Element* folded = l.doc ? l.doc->GetElementById("r10") : nullptr;
    CHECK(folded && folded->IsClassSet("ctl") && folded->GetAttribute<Rml::String>("data-ln", "") == "10");
    // laid out by the theme: the window 1270 x 850, the two columns side by side, as wide, inside the body
    CHECK(l.doc && std::fabs(l.doc->GetBox().GetSize(Rml::BoxArea::Border).x - 1270) < 1);
    CHECK(l.doc && std::fabs(l.doc->GetBox().GetSize(Rml::BoxArea::Border).y - 850) < 1);
    if (cols[0] && cols[1] && body) {
        const float w0 = cols[0]->GetBox().GetSize().x, w1 = cols[1]->GetBox().GetSize().x;
        CHECK(w0 > 400 && std::fabs(w0 - w1) < 2);
        CHECK(cols[1]->GetAbsoluteOffset().x > cols[0]->GetAbsoluteOffset().x + w0);
        CHECK(cols[1]->GetAbsoluteOffset().x + w1 <= body->GetAbsoluteOffset().x + body->GetBox().GetSize().x + 1);
    }
    rmlhost::close(l.doc);
}

TEST(uikit_reference_narrow_is_one_column) {
    ur::Look look;
    look.narrow = true;
    ur::Model m;
    m.narrow = true;
    m.body = ur::body(card(), look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    CHECK_EQ(count(l.doc, ".rcols"), size_t(0));
    CHECK_EQ(count(l.doc, ".rsec"), size_t(3));
    CHECK(l.doc && l.doc->IsClassSet("narrow"));
    rmlhost::close(l.doc);
}

TEST(uikit_reference_long_list_goes_across_in_columns) {
    for (const int rows : {30, 70}) {
        refpages::Page p;
        p.rows.push_back(row(Style::Heading, "З кого випадає"));
        for (int i = 0; i < rows; ++i) p.rows.push_back(row(Style::Row, "Mob", refpages::Go::Mob, 1000 + i));
        p.rows.push_back(row(Style::Heading, "Де купити"));
        p.rows.push_back(row(Style::Row, "Tool Dealer", refpages::Go::Shop, 1));
        ur::Model m;
        m.body = ur::body(p, ur::Look{});
        const Loaded l = load(m);
        CHECK_EQ(l.warnings, std::string());
        CHECK_EQ(count(l.doc, ".rsplit"), size_t(1));
        CHECK_EQ(count(l.doc, ".rspc"), size_t(rows > 60 ? 3 : 2));
        CHECK_EQ(count(l.doc, "[data-ln]"), size_t(rows + 1));
        Rml::Element* shop = l.doc ? l.doc->GetElementById("r" + std::to_string(rows + 2)) : nullptr;
        CHECK(shop && !within(shop, "rsplit"));  // the short one after it on its own
        rmlhost::close(l.doc);
    }
}

TEST(uikit_reference_text_escaped_and_marks) {
    refpages::Page p;
    p.rows.push_back(row(Style::Group, "Предмети · 2"));
    Row r = row(Style::Result, "Poring <b>& Co", refpages::Go::Item, 4001);
    r.marks = {{0, 6}};
    p.rows.push_back(r);
    p.rows.push_back(row(Style::Group, "NPC · 1"));
    p.rows.push_back(row(Style::Result, "Poring", refpages::Go::Map));
    ur::Model m;
    m.body = ur::body(p, ur::Look{});
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    CHECK_EQ(count(l.doc, "b"), size_t(0));  // the words' «<b>» stays words
    Rml::Element* mk = l.doc ? l.doc->QuerySelector("#r1 .mk") : nullptr;
    CHECK(mk && mk->GetInnerRML() == "Poring");
    CHECK(m.body.find("&lt;b&gt;&amp; Co") != std::string::npos);
    CHECK_EQ(count(l.doc, ".rjump .rjc"), size_t(2));  // two groups: the jump row over them
    Rml::Element* jump = l.doc ? l.doc->QuerySelector(".rjc") : nullptr;
    CHECK(jump && jump->GetAttribute<Rml::String>("data-jump", "") == "r0");
    rmlhost::close(l.doc);
}

TEST(uikit_reference_world_page_bar_map_panel) {
    refpages::Page p;
    Row tab = row(Style::Tab, "Світ", refpages::Go::World, 0);
    tab.gold = true;
    p.rows.push_back(tab);
    p.rows.push_back(row(Style::Tab, "Підземелля", refpages::Go::World, 1));
    Row tool = row(Style::Tool, "", refpages::Go::Zoom, 1);
    tool.icon = "ui:plus";
    p.rows.push_back(tool);
    p.rows.push_back(row(Style::World, ""));  // 3
    p.rows.push_back(row(Style::Caption, "Шлях до Payon"));
    Row step = row(Style::Step, "Payon Field");
    step.right = "1";
    step.tone = 1;
    p.rows.push_back(step);
    Row legend = row(Style::Legend, "Ви тут");
    legend.icon = "mark:here";
    p.rows.push_back(legend);
    ur::Look look;
    look.worldW = 600;
    look.worldH = 400;
    look.picture = [](const std::string& key) { return key == "world" ? std::string("pic:9:3") : std::string(); };
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    Rml::Element* map = l.doc ? l.doc->GetElementById("wmap") : nullptr;
    CHECK(map && within(map, "rwleft") && map->IsClassSet("ctl"));
    CHECK(l.doc && l.doc->GetElementById("wimg"));
    CHECK_EQ(count(l.doc, ".rwbar .rwtab"), size_t(2));
    CHECK_EQ(count(l.doc, ".rwbar .rwtool"), size_t(1));
    CHECK_EQ(count(l.doc, ".rwpanel .rstep.paid"), size_t(1));
    CHECK_EQ(count(l.doc, ".rwpanel .lg-here"), size_t(1));
    CHECK_EQ(count(l.doc, ".rwtab.on"), size_t(1));
    rmlhost::close(l.doc);
}

// 3.0.18-beta.240 (the reference's «одна карта»): a location's card - the map's bar (its layers, its zoom), the map
// box at its size with its picture and words, its NPCs beside it with the kinds' jump row; no picture box in the hero
TEST(uikit_reference_location_map_and_its_npcs) {
    refpages::Page p;
    p.rows.push_back(row(Style::Title, "Prontera"));
    p.rows.push_back(row(Style::Portrait, "Prontera City"));  // no picture: the map below is it
    Row view = row(Style::MapView, "prontera");
    p.rows.push_back(view);  // 2
    Row g1 = row(Style::Group, "Кафра · 2");
    g1.side = true;
    p.rows.push_back(g1);  // 3
    for (int i = 0; i < 2; ++i) {
        Row r = row(Style::Row, "Kafra", refpages::Go::Npc, i);
        r.side = true;
        r.dot = 0xE0447C;
        p.rows.push_back(r);  // 4, 5
    }
    Row g2 = row(Style::Group, "Квести · 1");
    g2.side = true;
    p.rows.push_back(g2);  // 6
    Row q = row(Style::Row, "Old Man", refpages::Go::Npc, 2);
    q.side = true;
    p.rows.push_back(q);  // 7
    p.rows.push_back(row(Style::Heading, "Хто тут"));  // 8
    p.rows.push_back(row(Style::Row, "Poring", refpages::Go::Mob, 1002));
    ur::Look look;
    look.mapW = 520;
    look.mapH = 480;
    look.layers = {{"NPC", true}, {"Переходи", true}, {"Монстри", false}, {"Ви тут", true}};
    look.mapWords = "<span class=\"ww\" style=\"left: 1px; top: 1px;\">K</span>";
    look.picture = [](const std::string& key) {
        return key == "mapview" ? std::string("pic:5:1") : key.rfind("ui:", 0) == 0 ? std::string("pic:6:1")
                                                                                     : std::string();
    };
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    Rml::Element* map = l.doc ? l.doc->GetElementById("mview") : nullptr;
    CHECK(map && within(map, "rmleft") && map->IsClassSet("ctl"));
    CHECK(l.doc && l.doc->GetElementById("mimg"));
    CHECK(map && std::fabs(map->GetBox().GetSize(Rml::BoxArea::Content).x - 520) < 1);
    CHECK_EQ(count(l.doc, ".rmbar .rlay"), size_t(4));
    CHECK_EQ(count(l.doc, ".rmbar .rlay.on"), size_t(3));
    CHECK_EQ(count(l.doc, ".rmbar [data-mz]"), size_t(3));
    CHECK_EQ(count(l.doc, ".rmview .ww"), size_t(1));
    CHECK_EQ(count(l.doc, ".rmside .rjc"), size_t(2));
    Rml::Element* oldMan = l.doc ? l.doc->GetElementById("r7") : nullptr;
    CHECK(oldMan && within(oldMan, "rmside"));
    Rml::Element* poring = l.doc ? l.doc->GetElementById("r9") : nullptr;
    CHECK(poring && !within(poring, "rmside") && !within(poring, "rmleft"));
    CHECK_EQ(count(l.doc, ".rpic"), size_t(0));
    rmlhost::close(l.doc);
}

// 3.0.18-beta.240: a portrait whose picture cannot be made (an NPC's sprite not read, a monster without a sprite) has
// no empty box - its words take the width
TEST(uikit_reference_portrait_without_its_picture_has_no_box) {
    refpages::Page p = card();
    p.rows[1].picture = "npc:83";
    ur::Look look;
    look.picture = [](const std::string&) { return std::string(); };
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    CHECK(l.doc && count(l.doc, ".rport") == 1 && count(l.doc, ".rpic") == 0);
    rmlhost::close(l.doc);
}
