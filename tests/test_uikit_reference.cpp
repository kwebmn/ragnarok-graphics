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
    // 3.0.18-beta.241: the pad's cross on a map, hidden until the pad takes the map
    Rml::Element* cross = l.doc ? l.doc->GetElementById("rcross") : nullptr;
    CHECK(cross && !cross->IsVisible() && cross->GetNumChildren() == 2);
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
    CHECK_EQ(count(body, "div.rpimg"), size_t(1));  // 3.0.18-beta.253: fitted into its box (a decorator's scale-down)
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
    // 3.0.18-beta.248 (the owner: «всюду немного покороченый интерфейс» - names cut in three columns): three only in a
    // wide window
    for (const auto& [rows, wide] : {std::pair<int, bool>{30, false}, {70, false}, {70, true}}) {
        refpages::Page p;
        p.rows.push_back(row(Style::Heading, "З кого випадає"));
        for (int i = 0; i < rows; ++i) p.rows.push_back(row(Style::Row, "Mob", refpages::Go::Mob, 1000 + i));
        p.rows.push_back(row(Style::Heading, "Де купити"));
        p.rows.push_back(row(Style::Row, "Tool Dealer", refpages::Go::Shop, 1));
        ur::Model m;
        ur::Look look;
        look.wide = wide;
        m.body = ur::body(p, look);
        const Loaded l = load(m);
        CHECK_EQ(l.warnings, std::string());
        CHECK_EQ(count(l.doc, ".rsplit"), size_t(1));
        CHECK_EQ(count(l.doc, ".rspc"), size_t(rows > 60 && wide ? 3 : 2));
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
    // 3.0.18-beta.254 (R-TMUEN4 «не всю карту показало»: a long way's panel made the page taller than the window and
    // the map was trimmed to a strip): the panel no taller than the map, its steps scrolled in it
    Rml::Element* panel = nullptr;
    if (l.doc) {
        Rml::ElementList found;
        l.doc->QuerySelectorAll(found, ".rwpanel");
        if (found.size() == 1) panel = found[0];
    }
    CHECK(panel && panel->GetProperty<float>("max-height") == 400.0f);
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

// 3.0.18-beta.241 (the concept's pad: the gold frame on rows): the pad walks the body's controls - every row that leads
// somewhere, the folded heading, the map's chips and the map - in the markup's order, none of the chrome's
TEST(uikit_reference_pad_targets_are_the_bodys_controls) {
    const refpages::Page p = card();
    ur::Look look;
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    Rml::Element* body = l.doc ? l.doc->GetElementById("body") : nullptr;
    const std::vector<Rml::Element*> t = ur::padTargets(body);
    CHECK_EQ(t.size(), size_t(7));
    bool rows = true;
    for (Rml::Element* e : t) rows = rows && within(e, "rcols") && e->HasAttribute("data-ln");
    CHECK(rows);
    CHECK(t.size() == 7 && t[0]->GetId() == "r3" && t[6]->GetId() == "r10");
    rmlhost::close(l.doc);
    // a location's page: its layers' chips, its zoom tools, its map, then its NPCs beside it
    refpages::Page loc;
    loc.rows.push_back(row(Style::Title, "Prontera"));
    loc.rows.push_back(row(Style::MapView, ""));
    loc.rows.push_back(row(Style::Row, "Kafra Employee", refpages::Go::Npc, 0));
    loc.rows.back().side = true;
    ur::Look ml;
    ml.mapW = 600;
    ml.mapH = 400;
    ml.layers = {{"NPC", true}, {"Переходи", true}};
    ur::Model mm;
    mm.body = ur::body(loc, ml);
    const Loaded ll = load(mm);
    const std::vector<Rml::Element*> lt = ur::padTargets(ll.doc ? ll.doc->GetElementById("body") : nullptr);
    int chips = 0, tools = 0, maps = 0, npcs = 0;
    for (Rml::Element* e : lt) {
        chips += e->HasAttribute("data-lay");
        tools += e->HasAttribute("data-mz");
        maps += e->GetId() == "mview";
        npcs += e->HasAttribute("data-ln");
    }
    CHECK(chips == 2 && tools == 3 && maps == 1 && npcs == 1);
    rmlhost::close(ll.doc);
}

// 3.0.18-beta.242 (the concept's cards): an item's words from the game under its name beside the picture, line by line;
// a monster's «Lv N» coloured by the character's level (1 fitting, 2 too strong, 3 too weak)
TEST(uikit_reference_description_and_level_tone) {
    refpages::Page p;
    p.rows.push_back(row(Style::Title, "Apple"));
    Row hero = row(Style::Portrait, "Вага 2");
    hero.sub = "A red <fruit>.\nHeals 16 HP.";
    p.rows.push_back(hero);
    p.rows.push_back(row(Style::Heading, "Хто дропає"));
    Row poring = row(Style::Row, "Poring", refpages::Go::Mob, 1002);
    poring.right = "10 % · Lv 1 · prt_fild08";
    poring.lvTone = 3;
    p.rows.push_back(poring);
    Row plain = row(Style::Row, "Lunatic", refpages::Go::Mob, 1063);
    plain.right = "5 % · Lv 3";
    p.rows.push_back(plain);
    ur::Look look;
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    Rml::Element* d = l.doc ? l.doc->QuerySelector(".rpd") : nullptr;
    const std::string words = d ? d->GetInnerRML() : std::string();
    CHECK(words.find("A red &lt;fruit&gt;.") != std::string::npos && words.find("<br") != std::string::npos &&
          words.find("Heals 16 HP.") != std::string::npos);
    Rml::Element* lv = l.doc ? l.doc->QuerySelector(".rlv.t3") : nullptr;
    CHECK(lv && lv->GetInnerRML() == "Lv 1");
    CHECK(l.doc && count(l.doc, ".rlv") == 1);  // a row without a tone: its words as they were
    rmlhost::close(l.doc);
}

// 3.0.18-beta.247 (R-ZW43KT): an icon not made yet is a placeholder that names its picture (data-pic) - the window
// puts the picture on it in place when it comes (the page's markup is not made again for it)
TEST(uikit_reference_icon_placeholders_name_their_pictures) {
    refpages::Page p;
    p.rows.push_back(row(Style::Heading, "Монстри"));
    Row poring = row(Style::Row, "Poring", refpages::Go::Mob, 1002);
    poring.icon = "mob:1002";
    p.rows.push_back(poring);
    Row wing = row(Style::Row, "Fly Wing", refpages::Go::Item, 601);
    wing.icon = "item:601";
    p.rows.push_back(wing);
    ur::Look look;
    look.picture = [](const std::string& key) {
        return key == "icon:item:601" ? std::string("pic:7:1") : std::string();
    };
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    Rml::Element* body = l.doc ? l.doc->GetElementById("body") : nullptr;
    Rml::ElementList waiting;
    if (body) body->QuerySelectorAll(waiting, "[data-pic]");
    CHECK(waiting.size() == 1 && waiting[0]->GetAttribute<Rml::String>("data-pic", "") == "icon:mob:1002");
    CHECK(waiting.size() == 1 && waiting[0]->IsClassSet("none"));
    CHECK(body && count(body, "img.ri") == 1);  // the one made: its picture at once
    if (waiting.size() == 1) {  // the window's way: the picture as the placeholder's decorator, RmlUi takes it
        const size_t before = rmlhost::log().size();
        waiting[0]->SetProperty("decorator", "image(\"pic:9:1\" contain)");
        waiting[0]->SetClass("none", false);
        rmlhost::update();
        CHECK(waiting[0]->GetProperty("decorator") != nullptr);
        CHECK_EQ(rmlhost::log().substr(before), std::string());
    }
    rmlhost::close(l.doc);
}

// 3.0.18-beta.250 (the owner: «картинки сделай побольше в базе»): the lists' pictures bigger - a monster's 44, a
// thing's and a skill's 32 (a chip's stays 22)
TEST(uikit_reference_list_pictures_bigger) {
    refpages::Page p;
    p.rows.push_back(row(Style::Heading, "Список"));
    Row poring = row(Style::Row, "Poring", refpages::Go::Mob, 1002);
    poring.icon = "mob:1002";
    p.rows.push_back(poring);
    Row wing = row(Style::Row, "Fly Wing", refpages::Go::Item, 601);
    wing.icon = "item:601";
    p.rows.push_back(wing);
    Row chip = row(Style::Chip, "Apple", refpages::Go::Item, 512);
    chip.icon = "item:512";
    p.rows.push_back(chip);
    ur::Look look;
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    const auto side = [&](const char* id) {
        Rml::Element* r = l.doc ? l.doc->GetElementById(id) : nullptr;
        Rml::Element* i = r ? r->QuerySelector(".ri") : nullptr;
        return i ? i->GetBox().GetSize().x : 0.0f;
    };
    CHECK(std::fabs(side("r1") - 44) < 0.5f);
    CHECK(std::fabs(side("r2") - 32) < 0.5f);
    CHECK(std::fabs(side("r3") - 22) < 0.5f);
    rmlhost::close(l.doc);
}

// 3.0.18-beta.253 (the owner's review of the reference: R-N7GPT5, R-U5HAJ6, R-9CD8YA): a list's entries - their
// names, the lines under them, their figures in columns (a value over its label); a card's facts beside its picture
TEST(uikit_reference_entries_and_facts) {
    refpages::Page p;
    p.rows.push_back(refpages::Row{});
    p.rows.back().style = refpages::Style::Portrait;
    p.rows.back().picture = "mob:1002";
    p.rows.back().cells = {refpages::Cell{"50", "HP", 0, false}, refpages::Cell{"Novice, Swordman", "Jobs", 0, true}};
    for (int i = 0; i < 3; ++i) {
        refpages::Row r;
        r.style = refpages::Style::Entry;
        r.left = "Poring " + std::to_string(i);
        r.sub = "Plant · Water 1 · Medium";
        r.icon = "mob:1002";
        r.link = refpages::Link{refpages::Go::Mob, 1002, ""};
        r.cells = {refpages::Cell{"1", "Lv", 1, false}, refpages::Cell{"50", "HP", 0, false},
                   refpages::Cell{"1 500 z", "у магазині", 0, true}};
        p.rows.push_back(std::move(r));
    }
    ur::Look look;
    look.picture = [](const std::string& key) { return key == "mob:1002" ? std::string("pic:1:1") : std::string(); };
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    Rml::Element* body = l.doc ? l.doc->GetElementById("body") : nullptr;
    CHECK(body != nullptr);
    CHECK_EQ(count(body, ".rent"), size_t(3));
    CHECK_EQ(count(body, ".rent .rec"), size_t(9));
    CHECK_EQ(count(body, ".rec.w"), size_t(3));
    CHECK_EQ(count(body, ".recv.t1"), size_t(3));
    CHECK_EQ(count(body, ".rport.hasfacts .rfact"), size_t(2));
    CHECK_EQ(count(body, ".rfact.wide"), size_t(1));
    CHECK_EQ(count(body, "[data-ln]"), size_t(3));
    rmlhost::close(l.doc);
}

// 3.0.18-beta.255 (the owner: «что падает не показывает шанс»): a row's note at its right (a drop's chance, a place's
// count) laid out whole beside its name, in a card's block of two columns
TEST(uikit_reference_a_rows_note_shown_beside_its_name) {
    refpages::Page p;
    p.rows.push_back(row(Style::Title, "Fabre"));
    Row head = row(Style::Heading, "Що випадає");
    head.share = 50;
    p.rows.push_back(head);
    Row drop = row(Style::Row, "Fluff", refpages::Go::Item, 914);
    drop.right = "20 %";
    drop.icon = "item:914";
    p.rows.push_back(drop);  // 2
    Row other = row(Style::Heading, "Де знайти");
    other.share = 50;
    p.rows.push_back(other);
    Row place = row(Style::Row, "Mushroom Farm (job_thief1)", refpages::Go::Map, 0);
    place.right = "×50";
    p.rows.push_back(place);  // 4
    ur::Look look;
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    const auto width = [&](const char* selector) {
        Rml::Element* e = l.doc ? l.doc->QuerySelector(selector) : nullptr;
        return e ? e->GetBox().GetSize(Rml::BoxArea::Border).x : -1.0f;
    };
    CHECK(width("#r2 .rrt") > 20.0f);  // «20 %» at 12.5dp: about 34
    CHECK(width("#r4 .rrt") > 15.0f);
    CHECK(width("#r2 .rl") > 20.0f);
    rmlhost::close(l.doc);
}

// 3.0.18-beta.255: an entry's name, its line and its figures, a card's facts - each with its width in RmlUi's layout
// (none collapsed as .253's row note was)
TEST(uikit_reference_entries_and_facts_have_their_widths) {
    refpages::Page p;
    Row hero = row(Style::Portrait, "Insect · Small");
    hero.picture = "mob:1007";
    hero.cells = {refpages::Cell{"63", "HP", 0, false}, refpages::Cell{"3", "Base EXP", 0, false},
                  refpages::Cell{"Novice, Swordman, Acolyte, and Merchant Classes", "Jobs", 0, true}};
    p.rows.push_back(hero);  // 0
    Row head = row(Style::Group, "Lv 1–10");
    head.right = "40";
    p.rows.push_back(head);  // 1
    Row e = row(Style::Entry, "Fabre", refpages::Go::Mob, 1007);
    e.sub = "Insect · Earth 1 · Small";
    e.icon = "mob:1007";
    e.cells = {refpages::Cell{"2", "Lv", 1, false}, refpages::Cell{"63", "HP", 0, false},
               refpages::Cell{"1 500 z", "у магазині", 0, true}};
    p.rows.push_back(e);  // 2
    ur::Look look;
    look.picture = [](const std::string& key) { return key == "mob:1007" ? std::string("pic:1:1") : std::string(); };
    ur::Model m;
    m.body = ur::body(p, look);
    const Loaded l = load(m);
    CHECK_EQ(l.warnings, std::string());
    const auto width = [&](const char* selector) {
        Rml::Element* e = l.doc ? l.doc->QuerySelector(selector) : nullptr;
        return e ? e->GetBox().GetSize(Rml::BoxArea::Border).x : -1.0f;
    };
    CHECK(width("#r2 .renn") > 30.0f);
    CHECK(width("#r2 .rens") > 60.0f);
    CHECK(width("#r2 .rec .recv") > 20.0f);
    CHECK(width("#r2 .rec.w .recv") > 50.0f);
    CHECK(width("#r0 .rfact") > 60.0f);
    CHECK(width("#r0 .rfact.wide") > 3.0f * width("#r0 .rfact"));
    rmlhost::close(l.doc);
}
