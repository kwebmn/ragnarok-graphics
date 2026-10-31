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

TEST(skill_window_words_dots_and_pad_hints) {
    const skillwin::Data d = knight();
    skillwin::Show sh;
    sh.tab = 1;
    const skillwin::Texts t = texts();
    skillwin::Layout L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    std::vector<skillwin::Words> w = skillwin::words(L, d, sh, t, measure);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const skillwin::Words& x) { return x.text == text; });
    };
    CHECK(has("Навички") && has("Alt+S") && has("Provoke") && has("5/10") && has("Вільних очок: 1") &&
          has("Swordman") && has(t.drag));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    skillwin::paint(p, L, d, sh);
    const panel::Box& dots = L.dots[1];  // Provoke: 5 of 10 lit
    const float step = dots.w / 10.0f;
    const auto at = [&](float x, float y) { return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))]; };
    const uint32_t lit = at(dots.x + step * 4.5f - 1, dots.y + dots.h / 2), dark = at(dots.x + step * 5.5f - 1,
                                                                                         dots.y + dots.h / 2);
    CHECK(((lit >> 16) & 0xFF) > 0xA0 && ((dark >> 16) & 0xFF) < 0x80);
    sh.pad = true;
    sh.focus = 1;
    L = skillwin::layout(0, 0, 1.0f, d, sh, t, measure);
    w = skillwin::words(L, d, sh, t, measure);
    CHECK(has("Застосувати") && has("+1 рівень") && has("Призначити") && has("Закрити") && !has(t.drag));
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

