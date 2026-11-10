#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/skills_live.hpp"

using namespace uagfx;
namespace kw = uagfx::skillwin;
namespace sl = uagfx::uikit::skills;

namespace {

kw::Texts texts() {
    kw::Texts t;
    t.title = "Навички";
    t.hotkey = "Alt+S";
    t.points = "Вільних очок: {1}";
    t.drag = "Перетягніть навичку на панель клавіш";
    t.enemy = "на ворога";
    t.passive = "пасивна";
    t.sp = "SP {1}";
    t.unlearned = "Не вивчено";
    t.list = "Список";
    t.tree = "Дерево";
    t.plan = "План";
    t.apply = "Застосувати";
    t.reset = "Скинути";
    t.sideHint = "Наведіть на навичку";
    return t;
}

float measure(const std::string& s, float px, bool bold) { return float(s.size()) * px * (bold ? 0.6f : 0.55f); }
std::string none(const panel::Picture*) { return {}; }
std::string glyph(int, float h, float& w) {
    w = h;
    return {};
}
panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }

kw::Data data() {
    kw::Data d;
    d.points = 3;
    kw::Tab tab;
    tab.name = "Swordman";
    for (int i = 0; i < 10; ++i) {
        kw::Skill k;
        k.id = 1 + i;
        k.name = "Skill " + std::to_string(i);
        k.most = 10;
        k.level = i == 1 ? 3 : 0;
        k.type = 1;
        k.use = 2;
        k.up = true;
        tab.skills.push_back(k);
    }
    kw::Tab second;
    second.name = "Novice";
    d.tabs = {tab, second};
    return d;
}

}  // namespace

// 3.0.18-beta.97: «Навички» laid out by RmlUi - the core hit test on its boxes finds the rows, «+», the use level's
// arrows, the tabs and the head's buttons; the scroll counted
TEST(uikit_skills_list_refit_finds_rows_pluses_and_arrows) {
    const kw::Data d = data();
    kw::Show sh;
    sh.scroll = 1;
    kw::Layout L = kw::layout(0, 0, 1, d, sh, texts(), measure);
    static sl::Model m;
    sl::fill(m, L, d, sh, texts(), measure, none, glyph);
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(sl::rml(), [&](Rml::DataModelConstructor& c) { sl::bind(c, m); }, 50, 40, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    CHECK(sl::refit(doc, L));
    CHECK(std::abs(float(L.w) - 982) < 2.5f);
    CHECK_EQ(L.first, 1);
    CHECK_EQ(L.rows.size(), size_t(8));
    CHECK(std::abs(L.rows[1].y - L.rows[0].y - 46) < 0.5f);
    kw::Hit h = kw::hitTest(L, mid(L.rows[2]).x - 200, mid(L.rows[2]).y);
    CHECK(h.part == kw::Part::Row);
    CHECK_EQ(h.index, 3);
    CHECK(L.pluses[0].w > 0);  // skill 1: a point, the server's leave
    h = kw::hitTest(L, mid(L.pluses[0]).x, mid(L.pluses[0]).y);
    CHECK(h.part == kw::Part::Plus);
    CHECK_EQ(h.index, 1);
    CHECK(L.less[0].w > 0);  // skill 1 learned to 3: its use level's arrows
    h = kw::hitTest(L, mid(L.less[0]).x, mid(L.less[0]).y);
    CHECK(h.part == kw::Part::Less);
    h = kw::hitTest(L, mid(L.more[0]).x, mid(L.more[0]).y);
    CHECK(h.part == kw::Part::More);
    CHECK(L.less[1].w <= 0);  // an unlearned one: none
    CHECK_EQ(L.tabs.size(), size_t(2));
    h = kw::hitTest(L, mid(L.tabs[1]).x, mid(L.tabs[1]).y);
    CHECK(h.part == kw::Part::Tab);
    CHECK_EQ(h.index, 1);
    CHECK(kw::hitTest(L, mid(L.viewTree).x, mid(L.viewTree).y).part == kw::Part::ViewTree);
    CHECK(kw::hitTest(L, mid(L.planButton).x, mid(L.planButton).y).part == kw::Part::Plan);
    CHECK(kw::hitTest(L, mid(L.close).x, mid(L.close).y).part == kw::Part::Close);
    CHECK(kw::hitTest(L, mid(L.scroll).x, mid(L.scroll).y).part == kw::Part::BarV);
    CHECK(L.side.x > L.rows[0].x + L.rows[0].w);
    CHECK(!m.sideWords.empty());  // «Наведіть на навичку» in the side panel
    rmlhost::close(doc);
}

// «Дерево»: the nodes keep core skillwin's places against the view RmlUi placed - moved with it, still hit
TEST(uikit_skills_tree_nodes_move_with_the_view) {
    kw::Data d = data();
    kw::Show sh;
    sh.tree = true;
    kw::Layout L = kw::layout(0, 0, 1, d, sh, texts(), measure);
    CHECK(!L.rows.empty());
    static sl::Model m;
    sl::fill(m, L, d, sh, texts(), measure, none, glyph);
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(sl::rml(), [&](Rml::DataModelConstructor& c) { sl::bind(c, m); }, 50, 40, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    const panel::Box coreView = L.view, coreRow = L.rows[0];
    CHECK(sl::refit(doc, L));
    CHECK(std::abs(float(L.w) - 1000) < 2.5f);
    CHECK(std::abs((L.rows[0].x - coreRow.x) - (L.view.x - coreView.x)) < 0.01f);
    CHECK(std::abs((L.rows[0].y - coreRow.y) - (L.view.y - coreView.y)) < 0.01f);
    const kw::Hit h = kw::hitTest(L, mid(L.rows[0]).x, mid(L.rows[0]).y);
    CHECK(h.part == kw::Part::Row || h.part == kw::Part::Plus);
    CHECK(!m.nodes.empty());
    CHECK(std::abs(m.nodes[0].x - (coreRow.x - coreView.x)) < 0.01f);  // against the view
    rmlhost::close(doc);
}
