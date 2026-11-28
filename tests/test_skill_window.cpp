// 3.0.11-beta.96 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's «Навыки»):
// «Навички» redrawn (core/skill_window) - tabs by job, a row a skill: its icon, name, what it is, its level's dots out
// of its most, «+» where a free point raises it, its use level's arrows; the free points and the hints under them
#include <algorithm>
#include <string>
#include <vector>

#include "core/skill_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

float measure(const std::string& t, float px, bool bold) { return float(t.size()) * px * (bold ? 0.6f : 0.55f); }

skillwin::Texts texts() {
    skillwin::Texts t;
    t.title = "Навички";
    t.hotkey = "Alt+S";
    t.points = "Вільних очок: {1}";
    t.drag = "Перетягніть навичку на панель клавіш";
    t.passive = "Пасивна";
    t.enemy = "На ворога";
    t.ground = "На землю";
    t.self = "На себе";
    t.ally = "На союзника";
    t.trap = "Пастка";
    t.sp = "SP {1}";
    t.range = "дальність {1}";
    t.needs = "Потрібно: {1}";
    t.unlearned = "Не вивчено";
    t.use = "Застосувати";
    t.raise = "+1 рівень";
    t.assign = "Призначити";
    t.close = "Закрити";
    return t;
}

skillwin::Skill skill(int id, const char* name, int type, int level, int most, bool up, int range = 1) {
    skillwin::Skill k;
    k.id = id;
    k.name = name;
    k.type = type;
    k.level = level;
    k.use = level;
    k.most = most;
    k.up = up;
    k.sp = type ? 15 : 0;
    k.range = range;
    return k;
}

skillwin::Data knight() {
    skillwin::Data d;
    d.points = 1;
    d.tabs.push_back({"Novice", {skill(1, "Basic Skill", 0, 9, 9, false), skill(142, "First Aid", 4, 1, 1, false)}});
    skillwin::Skill magnum = skill(7, "Magnum Break", 2, 0, 10, false);
    magnum.needs = "Bash 5";
    d.tabs.push_back(
        {"Swordman", {skill(5, "Bash", 1, 10, 10, false), skill(6, "Provoke", 1, 5, 10, true, 9), magnum}});
    skillwin::Tab k{"Knight", {}};
    for (int i = 0; i < 12; ++i) k.skills.push_back(skill(55 + i, "Knight skill", 1, 1, 10, true));
    d.tabs.push_back(k);
    return d;
}

bool inside(const panel::Box& outer, const panel::Box& inner) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.x + inner.w <= outer.x + outer.w + 1e-3f &&
           inner.y + inner.h <= outer.y + outer.h + 1e-3f;
}

}  // namespace

TEST(skill_window_info_lines) {
    const skillwin::Texts t = texts();
    const skillwin::Data d = knight();
    CHECK(skillwin::info(d.tabs[0].skills[0], t) == "Пасивна");
    CHECK(skillwin::info(d.tabs[1].skills[0], t) == "На ворога \xC2\xB7 SP 15");
    CHECK(skillwin::info(d.tabs[1].skills[1], t) == "На ворога \xC2\xB7 SP 15 \xC2\xB7 дальність 9");
    CHECK(skillwin::info(d.tabs[1].skills[2], t) == "Потрібно: Bash 5");
    skillwin::Skill none = skill(8, "Endure", 4, 0, 10, false);
    CHECK(skillwin::info(none, t) == "Не вивчено");
}

TEST(skill_window_raise_and_use) {
    const skillwin::Data d = knight();
    const skillwin::Skill &bash = d.tabs[1].skills[0], &provoke = d.tabs[1].skills[1], &basic = d.tabs[0].skills[0];
    CHECK(skillwin::canRaise(provoke, 1) && !skillwin::canRaise(provoke, 0));  // a free point, the server lets
    CHECK(!skillwin::canRaise(bash, 1));                                     // the server does not
    CHECK(skillwin::usable(bash) && !skillwin::usable(basic) && !skillwin::usable(d.tabs[1].skills[2]));
}

TEST(skill_window_layout_tabs_rows_and_buttons) {
    const skillwin::Data d = knight();
    skillwin::Show sh;
    sh.tab = 1;
    const skillwin::Layout L = skillwin::layout(100, 50, 1.0f, d, sh, texts(), measure);
    CHECK(L.tabs.size() == 3 && L.tabs[1].x > L.tabs[0].x + L.tabs[0].w - 1);
    CHECK_EQ(L.rows.size(), 3u);  // the Swordman's three
    CHECK(L.pluses[0].w == 0 && L.pluses[1].w > 0 && L.pluses[2].w == 0);  // Provoke's alone
    CHECK(L.less[0].w > 0 && L.less[1].w > 0 && L.less[2].w == 0);         // learned past level 1 and not passive
    for (const panel::Box& r : L.rows) CHECK(inside(L.glass, r));
    CHECK(L.footer.y >= L.rows.back().y + L.rows.back().h);
    const auto at = [&](const panel::Box& b) { return skillwin::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.pluses[1]).part == skillwin::Part::Plus && at(L.pluses[1]).index == 1);
    CHECK(at(L.tabs[2]).part == skillwin::Part::Tab && at(L.tabs[2]).index == 2);
    CHECK(at(L.close).part == skillwin::Part::Close);
    CHECK(at(L.less[0]).part == skillwin::Part::Less && at(L.more[1]).part == skillwin::Part::More);
    const skillwin::Hit row = skillwin::hitTest(L, L.rows[2].x + 60, L.rows[2].y + 10);
    CHECK(row.part == skillwin::Part::Row && row.index == 2);
}

TEST(skill_window_scrolls_past_eight_rows) {
    const skillwin::Data d = knight();
    skillwin::Show sh;
    sh.tab = 2;
    skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    CHECK(int(L.rows.size()) == skillwin::kRows && L.first == 0 && L.scroll.w > 0);
    sh.scroll = 4;
    L = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    CHECK_EQ(L.first, 4);
    CHECK(skillwin::hitTest(L, L.rows[0].x + 60, L.rows[0].y + 10).index == 4);
    sh.scroll = 10;  // past the end: the last eight
    CHECK_EQ(skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure).first, 4);
    CHECK_EQ(skillwin::keepInSight(11, 0, 12), 4);  // the focus at the bottom
    CHECK_EQ(skillwin::keepInSight(2, 4, 12), 2);   // above: up to it
    CHECK_EQ(skillwin::keepInSight(5, 4, 12), 4);   // in sight: kept
}

// 3.0.11-beta.106 (R-WCXUQP: «глюк с переносом скилов на панель, не переносятся» - the game's drag begun at the press
// never saw its button come up, the press and the release being the driver's): a press on a skill becomes a drag of
// the window's own once the cursor moved past 10 pixels (times its scale); let go, a drag gives its skill (the game's
// drag begun and let go there), a click nothing
TEST(skill_window_a_press_becomes_a_drag_past_ten_pixels) {
    skillwin::Press p;
    CHECK(skillwin::released(p) < 0);  // nothing pressed
    skillwin::pressed(p, 3, 100, 100);
    CHECK(!skillwin::moved(p, 108, 100, 1.0f));  // a hand's shake
    CHECK(skillwin::moved(p, 111, 100, 1.0f));
    CHECK(skillwin::moved(p, 100, 100, 1.0f));  // dragging till let go, back over the window too
    CHECK_EQ(skillwin::released(p), 3);
    CHECK(skillwin::released(p) < 0 && p.index < 0);  // let go once
    skillwin::pressed(p, 5, 100, 100);
    CHECK(!skillwin::moved(p, 115, 100, 2.0f));  // 20 pixels at the scale 2
    CHECK(skillwin::released(p) < 0);           // a click: no drag
}


// 3.0.11-beta.223 (the skills concept, stage 1: «Ще закриті»): the tree's skills the server has not sent, after the
// sent ones - why each is closed, never a «+»; a line over the list says why a whole job is closed
TEST(skill_window_closed_skills_and_the_notice) {
    skillwin::Texts t = texts();
    t.quest = "Квест — очками не вчиться";
    t.wedding = "Лише для подружжя";
    t.soulLink = "Лише під Soul Link";
    t.earlier = "Спершу витратьте очки {1}";
    skillwin::Skill bb = skill(62, "Bowling Bash", 1, 0, 10, true);
    bb.lock = skillwin::Lock::Needs;
    bb.needs = "Twohand Quicken 10";
    CHECK(skillwin::info(bb, t) == "Потрібно: Twohand Quicken 10" && !skillwin::canRaise(bb, 5));
    skillwin::Skill charge = skill(1001, "Charge Attack", 1, 0, 1, false);
    charge.lock = skillwin::Lock::Quest;
    CHECK(skillwin::info(charge, t) == t.quest);
    skillwin::Skill pierce = skill(56, "Pierce", 1, 0, 10, false);
    pierce.lock = skillwin::Lock::Earlier;
    pierce.lockJob = "Swordsman";
    CHECK(skillwin::info(pierce, t) == "Спершу витратьте очки Swordsman");
    skillwin::Data d = knight();
    d.tabs[1].skills.push_back(bb);
    d.notice = "Навички Knight відкриються, коли витратите вільні очки на навички Swordsman";
    skillwin::Show sh;
    sh.tab = 1;
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    CHECK(L.notice.w > 0 && L.list.y >= L.notice.y + L.notice.h && L.rows.size() == 4);
    CHECK(L.pluses[3].w == 0);  // the closed one: no «+»
    const std::vector<skillwin::Words> w = skillwin::words(L, d, sh, t, measure);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const skillwin::Words& x) { return x.text == text; });
    };
    CHECK(has(d.notice) && has("Потрібно: Twohand Quicken 10"));
    d.notice.clear();
    CHECK(skillwin::layout(0, 0, 1.0f, d, sh, t, measure).notice.w == 0);
}

// 3.0.24-beta.27 (docs/superpowers/plans/2026-11-27-other-windows-old-layout.md, task 2; the spec's «Навички (Alt+S)»:
// «Узкий список: значок, название, уровень, SP, стрелка «+1»; очки внизу; описание — отдельным окошком по ПКМ»): the
// list 420 wide, no side panel and no level dots (its «N/M» says it); «Дерево» as it was
TEST(skill_window_list_is_narrow) {
    const skillwin::Data d = knight();
    skillwin::Show sh;
    sh.tab = 1;
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    CHECK(L.title.w <= 420.0f && L.side.w == 0);
    CHECK(L.dots.size() == 3u && L.dots[1].w == 0);
    for (const panel::Box& r : L.rows) CHECK(r.x + r.w <= L.title.w);
    CHECK(L.less[1].x + L.less[1].w <= L.pluses[1].x);  // the use level's arrows left of the «+»
    sh.tree = true;
    const skillwin::Layout T = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    CHECK(T.title.w > 900.0f && T.side.w > 0);
}

// A name wider than its room ends in «…» within it
TEST(skill_window_long_name_fits) {
    const std::string n = skillwin::fitName("Improve Concentration of a Very Long Name", 120, 13, false, measure);
    CHECK(n.size() > 3 && n.compare(n.size() - 3, 3, "\xE2\x80\xA6") == 0);
    CHECK(measure(n, 13, false) <= 120.0f);
    CHECK_EQ(skillwin::fitName("Bash", 120, 13, false, measure), std::string("Bash"));
}

// The side panel's facts for the skill's card (a right click): its level, what it needs with each one met or how
// far, what it opens
TEST(skill_window_card_facts) {
    skillwin::Data d = knight();
    skillwin::Texts t = texts();
    t.levelOf = "Рівень {1} з {2}";
    t.needsHead = "Потрібно";
    t.unlocks = "Відкриває";
    skillwin::Skill& magnum = d.tabs[1].skills[2];
    magnum.req = {{5, 5}, {6, 6}};  // Bash 5 (at 10: met), Provoke 6 (at 5: not)
    magnum.level = 3;
    const std::vector<std::string> f = skillwin::cardFacts(d, t, 7);
    CHECK(f.size() == 4u);
    if (f.size() == 4u) {
        CHECK_EQ(f[0], std::string("Рівень 3 з 10"));
        CHECK_EQ(f[1], std::string("Потрібно:"));
        CHECK_EQ(f[2], std::string("Bash 5 \xE2\x9C\x93"));
        CHECK_EQ(f[3], std::string("Provoke 6 (5/6)"));
    }
    const std::vector<std::string> bash = skillwin::cardFacts(d, t, 5);
    CHECK(!bash.empty() && bash.back() == "Відкриває: Magnum Break");
    CHECK(skillwin::cardFacts(d, t, 9999).empty());
}

// 3.0.24-beta.30 (the deferred minors of part 3): a name's room measured in its row - from after the icon to the use
// level's arrows (or the plan's «−», else the level's column), not reckoned from the row's width
TEST(skill_window_name_room_measured) {
    const skillwin::Data d = knight();
    skillwin::Show sh;
    sh.tab = 1;
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    const float withArrows = skillwin::nameRoom(L, 1);  // Provoke: learned past 1 - its arrows
    CHECK(std::abs(withArrows - (L.less[1].x - (L.icons[1].x + L.icons[1].w) - 16)) < 0.5f);
    CHECK(skillwin::nameRoom(L, 2) > withArrows);  // Magnum Break: no arrows - room to the level's column
    CHECK(skillwin::nameRoom(L, 99) == 0);
}

// 3.0.24-beta.50 (the audit 28.11, item 3: a novice's two skills in a list eight rows tall): the list as tall as its
// biggest tab - every tab the same height (no jump between them) - at least kLeastRows, at most kRows; «Дерево» keeps
// its eight
TEST(skill_window_list_as_tall_as_its_biggest_tab) {
    skillwin::Data novice;
    novice.tabs.push_back(
        {"Novice", {skill(1, "Basic Skill", 0, 9, 9, false), skill(142, "First Aid", 4, 1, 1, false)}});
    skillwin::Show sh;
    const skillwin::Layout one = skillwin::layout(0, 0, 1.0f, novice, sh, texts(), measure);
    CHECK_EQ(one.shown, skillwin::kLeastRows);
    const skillwin::Data d = knight();  // tabs of 2, 3 and 12
    const skillwin::Layout a = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    sh.tab = 2;
    const skillwin::Layout b = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    CHECK(a.shown == skillwin::kRows && b.shown == skillwin::kRows && a.list.h == b.list.h);
    CHECK(one.list.h < a.list.h - 100);  // four rows of 46 against eight
    skillwin::Data six;
    six.tabs.push_back({"Novice", {skill(1, "Basic Skill", 0, 9, 9, false)}});
    skillwin::Tab acolyte{"Acolyte", {}};
    for (int i = 0; i < 6; ++i) acolyte.skills.push_back(skill(28 + i, "Acolyte skill", 1, 1, 10, true));
    six.tabs.push_back(acolyte);
    sh.tab = 0;
    const skillwin::Layout c = skillwin::layout(0, 0, 1.0f, six, sh, texts(), measure);
    CHECK_EQ(c.shown, 6);
    CHECK(c.scroll.w == 0);  // all six seen
    sh.tree = true;
    CHECK_EQ(skillwin::layout(0, 0, 1.0f, novice, sh, texts(), measure).shown, skillwin::kRows);
    CHECK_EQ(skillwin::keepInSight(5, 0, 8, 4), 2);  // four shown: the sixth at the bottom
}

// 3.0.24-beta.52 (the audit 28.11, item 10): the corner's rows the most the list shows - a big tab's twelve with 12
// chosen, a tab of six still six (no empty rows), never under kLeastRows
TEST(skill_window_corner_rows_cap_the_list) {
    skillwin::Data d = knight();  // tabs of 2, 3 and 12
    skillwin::Show sh;
    sh.rows = 12;
    sh.tab = 2;
    const skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    CHECK(L.shown == 12 && int(L.rows.size()) == 12 && L.scroll.w == 0);
    sh.rows = 5;
    const skillwin::Layout five = skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    CHECK(five.shown == 5 && five.scroll.w > 0);
    skillwin::Data six;
    skillwin::Tab t{"Acolyte", {}};
    for (int i = 0; i < 6; ++i) t.skills.push_back(skill(28 + i, "Acolyte skill", 1, 1, 10, true));
    six.tabs.push_back(t);
    sh.rows = 12;
    sh.tab = 0;
    CHECK_EQ(skillwin::layout(0, 0, 1.0f, six, sh, texts(), measure).shown, 6);
    sh.rows = 1;
    CHECK_EQ(skillwin::layout(0, 0, 1.0f, d, sh, texts(), measure).shown, skillwin::kLeastRows);
    CHECK(skillwin::rowStep(1.5f) > 60);  // a row's height for the corner (46 at 1)
}
