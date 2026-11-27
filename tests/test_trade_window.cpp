// 3.0.11-beta.116 (the windows plan's stage 7, «Обмін»): the trade redrawn (core/trade_window) - two sides of ten
// cells, the zeny under each, the buttons by the trade's state, the partner's changes after your «Готово» in red
#include <algorithm>
#include <string>
#include <vector>

#include "core/trade_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

tradewin::Texts tradeTexts() {
    tradewin::Texts t;
    t.title = "Обмін";
    t.partner = "Ліра";
    t.you = "Ви";
    t.zeny = "Зені";
    t.ready = "Готово";
    t.trade = "Обмін";
    t.cancel = "Скасувати";
    t.late = "Змінено після вашого «Готово»";
    t.hint = "Перетягніть речі з «Речі» або двічі клацніть";
    t.info = "ПКМ — картка";
    return t;
}

tradewin::Thing thing(int id, int count, const std::string& name) {
    tradewin::Thing x;
    x.id = id;
    x.count = count;
    x.name = name;
    return x;
}

bool hasWords(const std::vector<itemwin::Words>& w, const std::string& s) {
    return std::any_of(w.begin(), w.end(), [&](const itemwin::Words& x) { return x.text == s; });
}

}  // namespace

TEST(trade_window_layout_two_sides) {
    const tradewin::Layout L = tradewin::layout(0, 0, 1.0f);
    CHECK(L.w > 0 && L.h > 0);
    CHECK(L.cells[0].x < L.cells[10].x && L.cells[4].x + L.cells[4].w <= L.cells[10].x);  // yours left of theirs
    CHECK(L.cells[5].y > L.cells[0].y && L.cells[15].y > L.cells[10].y);                    // two rows of five
    CHECK(L.buttons[0].x < L.buttons[1].x && L.buttons[1].x < L.buttons[2].x);
    const auto at = [&](const panel::Box& b) { return tradewin::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.cells[0]).part == tradewin::Part::Cell && at(L.cells[0]).index == 0);
    CHECK(at(L.cells[12]).part == tradewin::Part::Cell && at(L.cells[12]).index == 12);
    CHECK(at(L.zenyField).part == tradewin::Part::Zeny);
    CHECK(at(L.buttons[1]).part == tradewin::Part::Button && at(L.buttons[1]).index == 1);
    CHECK(at(L.close).part == tradewin::Part::Close);
    CHECK(tradewin::hitTest(L, L.title.x + 30, L.title.y + 10).part == tradewin::Part::Title);
    CHECK(tradewin::hitTest(L, -5, -5).part == tradewin::Part::None);
}

TEST(trade_window_buttons_by_the_trades_state) {
    tradewin::Side me, them;
    CHECK(tradewin::canReady(me) && !tradewin::canTrade(me, them, false));
    me.ready = true;
    CHECK(!tradewin::canReady(me) && !tradewin::canTrade(me, them, false));  // the partner not ready yet
    them.ready = true;
    CHECK(tradewin::canTrade(me, them, false) && !tradewin::canTrade(me, them, true));  // pressed once
}

TEST(trade_window_the_partners_changes_after_your_ready) {
    tradewin::Side before, now;
    now.slots[0] = thing(1101, 1, "Sword");
    tradewin::Late late = tradewin::watchLate(before, now, false, tradewin::Late{});
    CHECK(!late.any());  // before your «Готово»: an ordinary offer
    before = now;
    now.slots[1] = thing(909, 10, "Jellopy");
    now.zeny = 5000;
    late = tradewin::watchLate(before, now, true, late);
    CHECK(late.slots[1] && late.zeny && !late.slots[0] && late.any());
    before = now;
    late = tradewin::watchLate(before, now, true, late);  // nothing new: the marks stay
    CHECK(late.slots[1] && late.zeny);
}


// 3.0.24-beta.24 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Обмін»: «красной рамкой — то,
// что другой положил (и зени, что выставил) за 5 с до своего «Готово»»; the plan's task 10): each of the partner's
// slots and their zeny keep when they last changed; at your «Готово» what changed within 5 s before it turns red
TEST(trade_window_the_partners_changes_just_before_your_ready) {
    tradewin::Side before, now;
    tradewin::Changed c;
    now.slots[0] = thing(1201, 1, "Knife");
    c = tradewin::stamp(before, now, 10000, c);  // the knife at 10 s
    before = now;
    now.slots[1] = thing(501, 5, "Red Potion");
    c = tradewin::stamp(before, now, 14000, c);  // the potions at 14 s
    before = now;
    now.zeny = 250000;
    c = tradewin::stamp(before, now, 15500, c);  // the zeny at 15.5 s
    const tradewin::Late l = tradewin::lateBefore(c, 17000, tradewin::Late{});  // your «Готово» at 17 s
    CHECK(!l.slots[0] && l.slots[1] && l.zeny && !l.slots[2]);  // 7 s before - not; 3 s and 1.5 s - red
    CHECK(!tradewin::lateBefore(tradewin::Changed{}, 17000, tradewin::Late{}).any());  // nothing changed
    const tradewin::Changed same = tradewin::stamp(now, now, 30000, c);  // a read with no change keeps the times
    CHECK(same.slots[1] == 14000u && same.zeny == 15500u && same.slots[0] == 10000u);
    tradewin::Late was;
    was.slots[3] = true;
    CHECK(tradewin::lateBefore(c, 17000, was).slots[3]);  // the marks already there stay
}
