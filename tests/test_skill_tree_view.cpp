// 3.0.11-beta.224 (the skills concept, stage 2: «Дерево»): a tab of «Навички» as a tree (core/skill_tree_view) -
// columns by the depth of the requirements on the tab, the earlier jobs' requirements as chips on the left, links
// with the levels they need; what the path to a closed skill costs; the window's view buttons and its scrolling
#include <algorithm>
#include <string>
#include <vector>

#include "core/skill_tree_view.hpp"
#include "core/skill_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

float measure(const std::string& t, float px, bool bold) { return float(t.size()) * px * (bold ? 0.6f : 0.55f); }

skillwin::Skill skill(int id, const char* name, int level, int most, std::vector<std::pair<int, int>> req = {}) {
    skillwin::Skill k;
    k.id = id;
    k.name = name;
    k.level = level;
    k.use = level;
    k.most = most;
    k.req = std::move(req);
    k.up = level < most;
    if (level == 0 && !k.req.empty()) k.lock = skillwin::Lock::Needs;
    return k;
}

// A Knight: Swordsman's Two-Handed 10, Bash 10, Magnum Break 3; Knight's own - Spear Mastery 1, Pierce (Spear Mastery
// 1), Spear Stab (Pierce 5), Twohand Quicken 10 (Two-Handed 1), Bowling Bash (Bash 10, Magnum 3, Two-Handed 5,
// Twohand Quicken 10)
skillwin::Data knight() {
    skillwin::Data d;
    d.points = 4;
    d.tabs.push_back({"Swordsman",
                      {skill(3, "Two-Handed Sword Mastery", 10, 10), skill(5, "Bash", 10, 10),
                       skill(7, "Magnum Break", 3, 10, {{5, 5}})}});
    skillwin::Skill pierce = skill(56, "Pierce", 0, 10, {{55, 1}});
    pierce.lock = skillwin::Lock::None;  // sent: open
    d.tabs.push_back({"Knight",
                      {skill(55, "Spear Mastery", 1, 10), pierce, skill(58, "Spear Stab", 0, 10, {{56, 5}}),
                       skill(60, "Twohand Quicken", 10, 10, {{3, 1}}),
                       skill(62, "Bowling Bash", 0, 10, {{5, 10}, {7, 3}, {3, 5}, {60, 10}})}});
    return d;
}

skillwin::Texts texts() {
    skillwin::Texts t;
    t.title = "Навички";
    t.hotkey = "Alt+S";
    t.points = "Вільних очок: {1}";
    t.drag = "Перетягніть на панель";
    t.needs = "Потрібно: {1}";
    t.list = "Список";
    t.tree = "Дерево";
    t.path = "До {1} — ще очок: {2}";
    return t;
}

}  // namespace

TEST(skill_tree_view_columns_chips_and_links) {
    const skillwin::Data d = knight();
    const skilltreeview::Plan p = skilltreeview::arrange(d, 1, 1.0f);
    CHECK(p.nodes.size() == 5);
    const auto col = [&](int index) { return p.nodes[size_t(index)].col; };
    CHECK(col(0) == 0 && col(1) == 1 && col(2) == 2 && col(3) == 0 && col(4) == 1);  // Bowling Bash after Quicken
    CHECK(p.nodes[1].row == p.nodes[0].row && p.nodes[2].row == p.nodes[1].row);    // a chain on one row
    // the chips: Two-Handed (for Quicken and Bowling Bash), Bash, Magnum Break - once each, in a column of their own
    CHECK(p.chips.size() == 3 && p.chips[0].id == 3 && p.chips[0].level == 10 && p.chips[0].name == "Two-Handed Sword "
                                                                                                   "Mastery");
    CHECK(p.nodes[0].box.x >= p.chips[0].box.x + p.chips[0].box.w);
    int toBowling = 0, met = 0;
    for (const skilltreeview::Link& l : p.links)
        if (l.to == 4) {
            ++toBowling;
            met += l.met;
        }
    CHECK(toBowling == 4 && met == 4);  // all met: Bowling Bash only waits for the server (a point)
    CHECK(p.w > p.nodes[2].box.x + p.nodes[2].box.w && p.h > p.chips[2].box.y);
    // twice the scale: twice the boxes
    const skilltreeview::Plan big = skilltreeview::arrange(d, 1, 2.0f);
    CHECK(std::abs(big.nodes[2].box.x - 2 * p.nodes[2].box.x) < 0.01f);
}

TEST(skill_tree_view_the_path_and_the_chain) {
    const skillwin::Data d = knight();
    CHECK_EQ(skilltreeview::levelOf(d, 60), 10);
    CHECK_EQ(skilltreeview::pathCost(d, 58), 6);   // Spear Stab: Pierce 0 -> 5, and itself 1
    CHECK_EQ(skilltreeview::pathCost(d, 62), 1);   // Bowling Bash: itself only
    CHECK_EQ(skilltreeview::pathCost(d, 55), 0);   // learned
    const std::set<int> c = skilltreeview::chain(d, 62);
    CHECK(c.count(5) && c.count(7) && c.count(3) && c.count(60) && !c.count(55));
}

TEST(skill_tree_view_in_the_window) {
    const skillwin::Data d = knight();
    const skillwin::Texts t = texts();
    skillwin::Show sh;
    sh.tab = 1;
    skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(!L.tree && L.viewList.w > 0 && L.viewTree.x > L.viewList.x && L.viewTree.x + L.viewTree.w < L.close.x);
    CHECK(skillwin::hitTest(L, L.viewTree.x + 3, L.viewTree.y + 3).part == skillwin::Part::ViewTree);
    sh.tree = true;
    L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.tree && L.rows.size() == 5 && L.view.w > 0 && L.w > 700);
    const panel::Box& pierce = L.rows[1];
    skillwin::Hit h = skillwin::hitTest(L, pierce.x + pierce.w / 2, pierce.y + pierce.h / 2);
    CHECK(h.part == skillwin::Part::Row && h.index == 1);
    CHECK(L.pluses[1].w > 0 && L.pluses[2].w == 0);  // Pierce: a point raises it; Spear Stab is closed
    h = skillwin::hitTest(L, L.pluses[1].x + L.pluses[1].w / 2, L.pluses[1].y + L.pluses[1].h / 2);
    CHECK(h.part == skillwin::Part::Plus && h.index == 1);
    CHECK(skillwin::hitTest(L, L.view.x + L.view.w - 3, L.view.y + L.view.h - 3).part == skillwin::Part::View);
    // the path line under it for the skill pointed at
    sh.hoverRow = 2;
    std::vector<skillwin::Words> w = skillwin::words(L, d, sh, t, measure);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const skillwin::Words& x) { return x.text == text; });
    };
    CHECK(has("До Spear Stab — ще очок: 6") && has("Список") && has("Дерево") && has("Spear Stab") && has("1/10"));
    CHECK(has("Bash 10") && !has(t.drag));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    skillwin::paint(p, L, d, sh);
    CHECK((p.px[size_t(pierce.y + pierce.h / 2) * size_t(p.width) + size_t(pierce.x + 2)] >> 24) > 0);
}

TEST(skill_tree_view_scrolls_and_keeps_a_skill_in_sight) {
    skillwin::Data d;
    d.points = 1;
    skillwin::Tab tab{"Monk", {}};
    tab.skills.push_back(skill(1000, "First", 1, 10));
    for (int i = 1; i < 9; ++i)  // a chain of nine: wider than the view
        tab.skills.push_back(skill(1000 + i, "Next", 0, 5, {{1000 + i - 1, 1}}));
    d.tabs.push_back(tab);
    skillwin::Show sh;
    sh.tree = true;
    const skillwin::Texts t = texts();
    skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.maxX > 0 && L.scrollH.w > 0 && L.scrollX == 0);
    CHECK(skillwin::treeInSight(sh, L, 8) && sh.treeX > 0);
    L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    const panel::Box& last = L.rows[8];
    CHECK(last.x + last.w <= L.view.x + L.view.w + 0.5f && !skillwin::treeInSight(sh, L, 8));
    // a skill out of the view: no words, no hit
    CHECK(skillwin::hitTest(L, L.rows[0].x + 3, L.rows[0].y + 3).part != skillwin::Part::Row);
    skillwin::scrollTree(sh, L, -100000, 0);
    CHECK(sh.treeX == 0);
}
