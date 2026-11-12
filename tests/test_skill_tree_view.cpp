// 3.0.11-beta.224 (the skills concept, stage 2: «Дерево»): a tab of «Навички» as a tree (core/skill_tree_view) -
// columns by the depth of the requirements on the tab, the earlier jobs' requirements as chips on the left, links
// with the levels they need; what the path to a closed skill costs; the window's view buttons and its scrolling
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/skill_plan.hpp"
#include "core/skill_tree.hpp"
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
    d.tabs[1].skills[4].lock = skillwin::Lock::None;  // all its requirements met: sent
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
    t.levelOf = "Рівень {1} з {2}";
    t.needsHead = "Потрібно";
    t.unlocks = "Відкриває";
    t.sideHint = "Наведіть на навичку";
    return t;
}

}  // namespace

TEST(skill_tree_view_columns_and_links) {
    const skillwin::Data d = knight();
    const skilltreeview::Plan p = skilltreeview::arrange(d, 1, 1.0f);
    CHECK(p.nodes.size() == 5);
    const auto col = [&](int index) { return p.nodes[size_t(index)].col; };
    CHECK(col(0) == 0 && col(1) == 1 && col(2) == 2 && col(3) == 0 && col(4) == 1);  // Bowling Bash after Quicken
    CHECK(p.nodes[1].row == p.nodes[0].row && p.nodes[2].row == p.nodes[1].row);    // a chain on one row
    // 3.0.11-beta.229: no chips - the jobs before's requirements only in the side panel; links on the tab only
    CHECK(p.chips.empty());
    int toBowling = 0;
    for (const skilltreeview::Link& l : p.links)
        if (l.to == 4) {
            ++toBowling;
            CHECK(l.from == 3 && l.met);  // Twohand Quicken 10
            const skilltreeview::Route r = skilltreeview::route(p, l, 1.0f);
            CHECK(r.n == 4 && r.x[0] == p.nodes[3].box.x + p.nodes[3].box.w && r.x[3] == p.nodes[4].box.x);
            CHECK(r.x[1] > r.x[0] && r.x[1] < r.x[3] && r.x[1] == r.x[2]);  // down the gutter between the columns
        }
    CHECK(toBowling == 1);
    CHECK(p.w > p.nodes[2].box.x + p.nodes[2].box.w);
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

// 3.0.18-beta.224 (R-3ZE2X9 «у рога плагіазм не відображається, скіли виходять за рамку»: the Rogue's tree wider than
// its view, Plagiarism («Intimidate» by the client's names) in its sixth column, the wheel moving nothing): a tree with
// no room to scroll up and down scrolls sideways by the plain wheel; Shift still sideways, a tall one up and down
TEST(skill_tree_view_the_wheel_scrolls_sideways_when_it_cannot_go_down) {
    skillwin::Data d;
    skillwin::Tab tab{"Rogue", {}};
    tab.skills.push_back(skill(1000, "First", 1, 10));
    for (int i = 1; i < 9; ++i)  // a chain of nine: wider than the view, one row high
        tab.skills.push_back(skill(1000 + i, "Next", 0, 5, {{1000 + i - 1, 1}}));
    d.tabs.push_back(tab);
    skillwin::Show sh;
    sh.tree = true;
    const skillwin::Texts t = texts();
    skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.maxX > 0 && L.maxY == 0);
    skillwin::wheelTree(sh, L, 60, false);
    CHECK(sh.treeX > 0 && sh.treeY == 0);  // sideways: the hidden columns come in
    const float x = sh.treeX;
    L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    skillwin::wheelTree(sh, L, -60, true);  // Shift: sideways back
    CHECK(sh.treeX < x);
}

// 3.0.11-beta.225 (the skills concept, stage 3: «План»): levels planned without asking the server - a requirement
// counts by its planned level, taking a level off takes off what needed it, the points the plan leaves; «Застосувати»
// asks the raises the server lets now, one by one; the plan's buttons in the window
TEST(skill_plan_levels_points_and_requirements) {
    skillwin::Data d = knight();
    d.tabs[1].skills[2].lock = skillwin::Lock::Needs;  // Spear Stab: Pierce 5 needed
    CHECK(!skillplan::canPlan(d, d.tabs[1].skills[2]));
    for (int i = 0; i < 4; ++i) CHECK(skillplan::up(d, 56));  // Pierce 0 -> 4 of the 4 free points
    CHECK(!skillplan::up(d, 56) && skillplan::left(d) == 0 && skillplan::cost(d) == 4);
    d.points = 10;
    CHECK(skillplan::up(d, 56) && skillplan::planLevel(d, 56) == 5);
    CHECK(skillplan::up(d, 58) && skillplan::up(d, 58) && skillplan::cost(d) == 7);  // Spear Stab by the planned Pierce
    CHECK(skillplan::down(d, 56) && skillplan::planLevel(d, 58) == 0 && skillplan::cost(d) == 4);  // and it went
    CHECK(!skillplan::down(d, 58));
    skillwin::Skill quest;
    quest.id = 1001;
    quest.most = 1;
    quest.lock = skillwin::Lock::Quest;
    d.tabs[1].skills.push_back(quest);
    CHECK(!skillplan::up(d, 1001));
    // kept as targets, back onto the skills; a level learned since drops out of the plan
    const skillplan::Targets t = skillplan::taken(d);
    CHECK(t.size() == 1 && t.at(56) == 4);
    d.tabs[1].skills[1].target = 0;
    d.tabs[1].skills[1].level = 4;
    skillplan::put(d, t);
    CHECK(skillplan::extra(d.tabs[1].skills[1]) == 0 && skillplan::cost(d) == 0);
}

TEST(skill_plan_applies_what_the_server_lets_raise) {
    skillwin::Data d = knight();
    d.tabs[1].skills[2].lock = skillwin::Lock::Needs;
    CHECK(skillplan::up(d, 56) && skillplan::up(d, 62));
    CHECK_EQ(skillplan::nextToApply(d), 56);  // Pierce first (the server's order)
    d.tabs[1].skills[1].level = 1;            // learned
    CHECK_EQ(skillplan::nextToApply(d), 62);
    d.tabs[1].skills[4].up = false;  // the server does not let it
    CHECK_EQ(skillplan::nextToApply(d), -1);
    d.tabs[1].skills[4].up = true;
    d.points = 0;
    CHECK_EQ(skillplan::nextToApply(d), -1);
}

TEST(skill_plan_in_the_window) {
    skillwin::Data d = knight();
    skillwin::Texts t = texts();
    t.plan = "План";
    t.apply = "Застосувати";
    t.applying = "Застосовується…";
    t.reset = "Скинути";
    t.planned = "у плані: {1}";
    skillwin::Show sh;
    sh.tab = 1;
    skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.planButton.w > 0 && L.planButton.x + L.planButton.w < L.viewList.x && L.apply.w == 0);
    CHECK(skillwin::hitTest(L, L.planButton.x + 3, L.planButton.y + 3).part == skillwin::Part::Plan);
    sh.plan = true;
    CHECK(skillplan::up(d, 56));
    L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.apply.w > 0 && L.reset.x + L.reset.w < L.apply.x);
    CHECK(L.minuses[1].w > 0 && L.minuses[0].w == 0);  // Pierce planned: its «−»
    CHECK(skillwin::hitTest(L, L.minuses[1].x + 3, L.minuses[1].y + 3).part == skillwin::Part::Minus);
    std::vector<skillwin::Words> w = skillwin::words(L, d, sh, t, measure);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const skillwin::Words& x) { return x.text == text; });
    };
    CHECK(has("Вільних очок: 3 · у плані: 1") && has("Застосувати") && has("Скинути") && has("1/10"));
    sh.tree = true;
    L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.minuses[1].w > 0 && L.pluses[1].w > 0 && L.minuses[1].x < L.pluses[1].x);
}

// 3.0.11-beta.226 (the skills concept, stage 4: «Наступна»): the jobs a character may change to next, and the tab of
// the next job's new skills - two jobs as buttons over it, one shown
TEST(skill_next_jobs_and_their_tab) {
    CHECK(skilltree::nextJobs(1, true) == std::vector<int>({7, 14}));      // Swordman: Knight, Crusader
    CHECK(skilltree::nextJobs(3, true) == std::vector<int>({11, 19}));     // a man: Hunter, Bard
    CHECK(skilltree::nextJobs(3, false) == std::vector<int>({11, 20}));    // a woman: Hunter, Dancer
    CHECK(skilltree::nextJobs(7, true) == std::vector<int>({4008}));       // Knight: Lord Knight
    CHECK(skilltree::nextJobs(4046, true) == std::vector<int>({4047, 4049}));
    CHECK(skilltree::nextJobs(0, true).empty() && skilltree::nextJobs(4008, true).empty());
    for (int j : {7, 14, 4008, 4047, 4049, 4030, 4043})  // every next job has its tree
        CHECK(!skilltree::skills(j).empty());
    skillwin::Data d = knight();
    skillwin::Tab next;
    next.name = "Наступна";
    next.next = true;
    next.choices = {"Knight", "Crusader"};
    next.chosen = 1;
    skillwin::Skill shield = skill(250, "Smite", 0, 5, {{3, 1}});
    shield.lock = skillwin::Lock::NextJob;
    next.skills.push_back(shield);
    d.tabs.push_back(next);
    skillwin::Texts t = texts();
    t.afterChange = "Після зміни професії";
    CHECK(skillwin::info(shield, t) == t.afterChange && skillplan::canPlan(d, shield));
    skillwin::Show sh;
    sh.tab = 2;
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.choices.size() == 2 && L.choices[1].x > L.choices[0].x && L.list.y > L.choices[0].y + L.choices[0].h);
    const skillwin::Hit h = skillwin::hitTest(L, L.choices[1].x + 3, L.choices[1].y + 3);
    CHECK(h.part == skillwin::Part::Choice && h.index == 1);
    CHECK(L.pluses[0].w == 0);  // nothing to raise before the change
    sh.tab = 1;
    CHECK(skillwin::layout(0, 0, 1.0f, d, sh, t, measure).choices.empty());
}

// 3.0.11-beta.227 (the skills concept, stage 5: the controller in «Дерево»): the D-pad goes to the nearest skill the
// way it points, staying put at the tree's edge
TEST(skill_tree_view_the_d_pad_goes_the_way_it_points) {
    const skillwin::Data d = knight();
    skillwin::Show sh;
    sh.tab = 1;
    sh.tree = true;
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    // Spear Mastery (0) - Pierce (1) - Spear Stab (2) on one row; Twohand Quicken (3) under Spear Mastery, Bowling
    // Bash (4) right of it
    CHECK_EQ(skillwin::treeStep(L, 0, 1, 0), 1);
    CHECK_EQ(skillwin::treeStep(L, 1, 1, 0), 2);
    CHECK_EQ(skillwin::treeStep(L, 2, 1, 0), 2);  // the edge
    CHECK_EQ(skillwin::treeStep(L, 0, 0, 1), 3);
    CHECK_EQ(skillwin::treeStep(L, 3, 1, 0), 4);
    CHECK_EQ(skillwin::treeStep(L, 4, -1, 0), 3);
    CHECK_EQ(skillwin::treeStep(L, 3, 0, -1), 0);
}

// 3.0.11-beta.229 (R-EQDQWH: «дерево в котором невозможно разобраться»): a link past the next column runs along the
// gap over its skill's row, never under the skills between
TEST(skill_tree_view_a_far_link_runs_through_the_gaps) {
    skillwin::Data d;
    d.points = 1;
    skillwin::Tab tab{"Knight", {}};
    tab.skills.push_back(skill(63, "Peco Peco Ride", 1, 1));
    tab.skills.push_back(skill(55, "Spear Mastery", 1, 10));
    tab.skills.push_back(skill(56, "Pierce", 1, 10, {{55, 1}}));
    tab.skills.push_back(skill(58, "Spear Stab", 3, 10, {{56, 1}}));
    tab.skills.push_back(skill(57, "Brandish Spear", 0, 10, {{63, 1}, {58, 3}}));
    d.tabs.push_back(tab);
    const skilltreeview::Plan p = skilltreeview::arrange(d, 0, 1.0f);
    const skilltreeview::Node& ride = p.nodes[0];
    const skilltreeview::Node& brandish = p.nodes[4];
    CHECK(brandish.col == 3 && ride.col == 0);
    for (const skilltreeview::Link& l : p.links) {
        if (l.from != 0 || l.to != 4) continue;
        const skilltreeview::Route r = skilltreeview::route(p, l, 1.0f);
        CHECK(r.n == 6 && r.y[2] == r.y[3] && r.y[2] < brandish.box.y && r.y[2] > brandish.box.y - 10);
        for (const skilltreeview::Node& n : p.nodes)  // the lane along the gap crosses no skill
            CHECK(!(r.y[2] > n.box.y && r.y[2] < n.box.y + n.box.h && n.box.x < r.x[3] && n.box.x + n.box.w > r.x[2]));
    }
}

// 3.0.11-beta.230: the side panel in «Список» too - the list as wide as before, the panel right of it with the skill
// pointed at: SP by level (the learned one lit) and the game's description
TEST(skill_window_side_panel_in_the_list) {
    skillwin::Data d = knight();
    d.tabs[1].skills[3].spLevels = {14, 18, 22, 26, 30, 34, 38, 42, 46, 50};
    d.tabs[1].skills[3].desc = {"Faster attacks with a two-handed sword.", "", "Lv 1: 30 s"};
    skillwin::Texts t = texts();
    t.spByLevel = "SP за рівнями";
    skillwin::Show sh;
    sh.tab = 1;
    sh.hoverRow = 3;
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(!L.tree && L.side.w > 0 && L.side.x >= L.list.x + L.list.w && std::abs(L.list.w - 668) < 1);
    const std::vector<skillwin::Words> w = skillwin::words(L, d, sh, t, measure);
    const auto find = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const skillwin::Words& x) { return x.text == text; });
    };
    CHECK(find("SP за рівнями") != w.end() && find("1: 14") != w.end() && find("10: 50") != w.end());
    CHECK(find("10: 50")->bold && !find("1: 14")->bold);  // its learned level, 10
    CHECK(find("Faster attacks with a two-handed sword.") != w.end() && find("Lv 1: 30 s") != w.end());
    CHECK(find("Twohand Quicken") != w.end() && find("Рівень 10 з 10") != w.end());
}

// 3.0.11-beta.245 (R-YCYYXU: «скрол не крутится внизу»): the scroll bars taken by the mouse - the tree's level one and
// upright one, the list's
TEST(skill_window_scroll_bars_by_the_mouse) {
    skillwin::Data d;
    d.points = 1;
    skillwin::Tab tab{"Monk", {}};
    tab.skills.push_back(skill(1000, "First", 1, 10));
    for (int i = 1; i < 9; ++i) tab.skills.push_back(skill(1000 + i, "Next", 0, 5, {{1000 + i - 1, 1}}));
    d.tabs.push_back(tab);
    skillwin::Show sh;
    sh.tree = true;
    const skillwin::Texts t = texts();
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.scrollH.w > 0);
    CHECK(skillwin::hitTest(L, L.scrollH.x + L.scrollH.w / 2, L.scrollH.y + 1).part == skillwin::Part::BarH);
    skillwin::barTo(sh, L, d, true, L.scrollH.x + L.scrollH.w, L.scrollH.y);  // the far end: all the way
    CHECK(std::abs(sh.treeX - L.maxX) < 0.5f);
    skillwin::barTo(sh, L, d, true, L.scrollH.x, L.scrollH.y);
    CHECK(sh.treeX == 0);
    // the list's
    skillwin::Data many;
    many.points = 0;
    skillwin::Tab big{"Knight", {}};
    for (int i = 0; i < 20; ++i) big.skills.push_back(skill(2000 + i, "S", 1, 10));
    many.tabs.push_back(big);
    skillwin::Show ls;
    const skillwin::Layout M = skillwin::layout(0, 0, 1.0f, many, ls, t, measure);
    CHECK(M.scroll.w > 0 && skillwin::hitTest(M, M.scroll.x + 1, M.scroll.y + 5).part == skillwin::Part::BarV);
    skillwin::barTo(ls, M, many, false, M.scroll.x, M.scroll.y + M.scroll.h);
    CHECK(ls.scroll == 20 - skillwin::kRows);
}

// 3.0.18-beta.29 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5): uaUI's «Дерево» - its links,
// nodes, icons and «+» as paintTree() draws them, in the window's pixels (the view's scroll counted)
TEST(skill_tree_view_parts_for_uaui) {
    const skillwin::Data d = knight();
    const skillwin::Texts t = texts();
    skillwin::Show sh;
    sh.tab = 1;
    sh.tree = true;
    sh.hoverRow = 2;
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    const skillwin::TreeView v = skillwin::treeView(L, d, sh);
    CHECK(v.nodes.size() == L.plan.nodes.size() && !v.links.empty());
    const skillwin::TreeNode& pierce = v.nodes[1];
    CHECK(std::fabs(pierce.box.x - L.rows[1].x) < 0.6f && std::fabs(pierce.box.y - L.rows[1].y) < 0.6f);
    CHECK(pierce.plus.w > 0 && v.nodes[2].plus.w == 0);  // Spear Stab is closed
    bool gold = false;  // the path to the skill pointed at in gold
    for (const skillwin::TreeLink& l : v.links) gold = gold || (l.rgb == 0xF3C653u && l.alpha > 0.9f);
    CHECK(gold);
}
