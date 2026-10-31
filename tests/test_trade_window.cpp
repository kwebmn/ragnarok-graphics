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

TEST(trade_window_words) {
    const tradewin::Layout L = tradewin::layout(0, 0, 1.0f);
    const tradewin::Texts t = tradeTexts();
    tradewin::Side me, them;
    me.slots[0] = thing(501, 38, "Red Potion");
    me.zeny = 12000;
    them.slots[0] = thing(1101, 1, "Sword");
    them.zeny = 1482300;
    them.ready = true;
    tradewin::Show sh;
    const itemwin::Measure m(panel::wordsWidth);
    std::vector<itemwin::Words> w = tradewin::words(L, me, them, sh, t, m);
    CHECK(hasWords(w, "Обмін") && hasWords(w, "Ліра") && hasWords(w, "Ви") && hasWords(w, "Скасувати"));
    CHECK(hasWords(w, "38") && hasWords(w, "12 000") && hasWords(w, "1 482 300"));
    CHECK(hasWords(w, "\xE2\x9C\x93 Готово"));  // the partner's mark
    CHECK(hasWords(w, t.hint) && !hasWords(w, "\xE2\x9A\xA0 " + t.late));
    me.ready = true;
    sh.late.zeny = true;
    w = tradewin::words(L, me, them, sh, t, m);
    CHECK(!hasWords(w, t.hint) && hasWords(w, "\xE2\x9A\xA0 " + t.late));
    const auto zeny = std::find_if(w.begin(), w.end(), [](const itemwin::Words& x) { return x.text == "1 482 300"; });
    CHECK(zeny != w.end() && zeny->rgb == 0xFF7D6E);  // changed after your «Готово»: red
    sh.hover = 10;  // the partner's sword pointed at: its tip
    w = tradewin::words(L, me, them, sh, t, m);
    CHECK(hasWords(w, "Sword") && hasWords(w, t.info));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    tradewin::paint(p, L, me, them, sh, t, m);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + 4] >> 24) > 0xC0);  // the glass
}
