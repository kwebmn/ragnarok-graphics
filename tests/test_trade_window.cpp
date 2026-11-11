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

// 3.0.18-beta.28 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5): uaUI's trade - the parts paint()
// draws, placed: the ready pill, the cells (late in red), the count plates, the buttons by the trade's state, the tip
TEST(trade_window_view_places_the_pictures_parts) {
    const tradewin::Layout L = tradewin::layout(0, 0, 1.0f);
    const tradewin::Texts t = tradeTexts();
    tradewin::Side me, them;
    me.slots[0] = thing(501, 38, "Red Potion");
    them.slots[0] = thing(1101, 1, "Sword");
    them.ready = true;
    tradewin::Show sh;
    sh.hover = 10;
    sh.late.slots[0] = true;
    const itemwin::Measure m(panel::wordsWidth);
    const tradewin::View v = tradewin::view(L, me, them, sh, t, m);
    CHECK(v.ready[0].w == 0 && v.ready[1].w > 0 && v.ready[1].x + v.ready[1].w <= L.theirHead.x + L.theirHead.w);
    CHECK(v.cells[0].count.w > 0 && v.cells[10].count.w == 0 && v.cells[1].thing == false);
    CHECK(v.cells[10].late && v.cells[10].hot && !v.cells[0].late);
    CHECK(v.buttons[0].on && v.buttons[0].main && !v.buttons[1].on && v.buttons[2].on && !v.buttons[2].main);
    CHECK(v.tip.w > 0 && v.dropping.w == 0);
    sh.dropping = true;
    CHECK(tradewin::view(L, me, them, sh, t, m).dropping.w > 0);
}
