// 3.0.11-beta.157 (task 7 of the remaining windows: identify and the card's compound): the game's «choose a thing»
// windows (the magnifier's list - UI window 0x49, a card's list of things it goes into - 0x4A) as one card
// (core/pick_card): a row a thing, the chosen one outlined, OK and cancel the game's own (0x76 / 0x77)
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "core/pick_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

TEST(pick_card_buttons_are_the_games) {
    CHECK(pickcard::kOkButton == 0x76 && pickcard::kCancelButton == 0x77);
}

// At the middle; a row a thing (7 shown at most, the wheel for the rest), OK and cancel under them; the mouse on each
TEST(pick_card_places_and_the_mouse) {
    const pickcard::Layout L = pickcard::layout(1920, 1080, 1.0f, 3);
    CHECK(L.rows.size() == 3);
    CHECK(L.x + L.w / 2 > 900 && L.x + L.w / 2 < 1020);
    CHECK(L.ok.y > L.rows[2].y && L.cancel.y == L.ok.y);
    using P = pickcard::Part;
    const pickcard::Hit row = pickcard::hitTest(L, 0, 3, mid(L.rows[1]).first, mid(L.rows[1]).second);
    CHECK(row.part == P::Row && row.row == 1);
    CHECK(pickcard::hitTest(L, 0, 3, mid(L.ok).first, mid(L.ok).second).part == P::Ok);
    CHECK(pickcard::hitTest(L, 0, 3, mid(L.cancel).first, mid(L.cancel).second).part == P::Cancel);
    CHECK(pickcard::hitTest(L, 0, 3, -5, -5).part == P::None);
    const pickcard::Layout many = pickcard::layout(1920, 1080, 1.0f, 12);
    CHECK(many.rows.size() == 7 && pickcard::mostScroll(many, 12) == 5);
    const pickcard::Hit scrolled = pickcard::hitTest(many, 4, 12, mid(many.rows[0]).first, mid(many.rows[0]).second);
    CHECK(scrolled.part == P::Row && scrolled.row == 4);
    CHECK(pickcard::layout(1920, 1080, 1.0f, 0).rows.empty());  // nothing to choose: the empty line instead
}

// Its words: the title and the line under it, each thing's name (+7 shown), OK's and cancel's; nothing to choose - the
// empty line; its ground painted
TEST(pick_card_words_and_picture) {
    std::vector<pickcard::Entry> e(2);
    e[0].name = "Unknown Sword";
    e[1].name = "Knife";
    e[1].refine = 7;
    pickcard::Texts t;
    t.title = "Опізнати річ";
    t.message = "Оберіть річ, яку опізнати";
    t.ok = "Опізнати";
    t.cancel = "Скасувати";
    t.empty = "Немає відповідних речей";
    const pickcard::Layout L = pickcard::layout(1920, 1080, 1.0f, e.size());
    pickcard::Show sh;
    sh.selected = 1;
    const std::vector<pickcard::Words> w = pickcard::words(L, e, sh, t);
    const auto has = [&](const std::vector<pickcard::Words>& ws, const std::string& s) {
        return std::any_of(ws.begin(), ws.end(), [&](const pickcard::Words& x) { return x.text == s; });
    };
    CHECK(has(w, "Опізнати річ") && has(w, "Оберіть річ, яку опізнати") && has(w, "Unknown Sword"));
    CHECK(has(w, "+7 Knife") && has(w, "Опізнати") && has(w, "Скасувати") && !has(w, "Немає відповідних речей"));
    const pickcard::Layout none = pickcard::layout(1920, 1080, 1.0f, 0);
    CHECK(has(pickcard::words(none, {}, pickcard::Show{}, t), "Немає відповідних речей"));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    pickcard::paint(p, L, e, sh);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);
}

// 3.0.11-beta.187 (the tester: «мені не вистачає скрола тут»): more rows than the seven shown - a bar at the list's
// right, its thumb as long as the share shown and where the scroll is, the rows narrower for it; a row with no icon
// (the servers') no empty square, its name from the row's left
TEST(pick_card_scroll_bar_and_rows_without_icons) {
    const pickcard::Layout few = pickcard::layout(1920, 1080, 1.0f, 7);
    CHECK(few.bar.w == 0 && pickcard::thumb(few, 0, 7).h == 0);
    const pickcard::Layout many = pickcard::layout(1920, 1080, 1.0f, 12);
    CHECK(many.bar.w > 0 && many.bar.x >= many.rows[0].x + many.rows[0].w);
    CHECK(many.bar.x + many.bar.w <= many.list.x + many.list.w + 0.5f);
    CHECK(std::fabs(many.bar.y - many.list.y) < 0.5f && std::fabs(many.bar.h - many.list.h) < 0.5f);
    const panel::Box top = pickcard::thumb(many, 0, 12), bottom = pickcard::thumb(many, 5, 12);
    CHECK(std::fabs(top.y - many.bar.y) < 0.5f);
    CHECK(std::fabs(bottom.y + bottom.h - (many.bar.y + many.bar.h)) < 0.5f);
    CHECK(std::fabs(top.h - many.bar.h * 7.0f / 12.0f) < 1.0f && std::fabs(bottom.h - top.h) < 0.5f);
    std::vector<pickcard::Entry> e(12);
    for (size_t i = 0; i < e.size(); ++i) e[i].name = "server " + std::to_string(i);
    const std::vector<pickcard::Words> w = pickcard::words(many, e, pickcard::Show{}, pickcard::Texts{});
    const auto it = std::find_if(w.begin(), w.end(), [](const pickcard::Words& x) { return x.text == "server 0"; });
    CHECK(it != w.end());
    if (it != w.end()) CHECK(it->box.x < many.rows[0].x + 20);
    panel::Picture p;
    p.width = many.w;
    p.height = many.h;
    p.px.assign(size_t(many.w) * size_t(many.h), 0u);
    pickcard::paint(p, many, e, pickcard::Show{});
    const auto at = [&](float x, float y) { return p.px[size_t(y) * size_t(p.width) + size_t(x)]; };
    const panel::Box& r = many.rows[0];
    CHECK(at(r.x + 6 + 14, r.y + r.h / 2) == at(r.x + r.w - 40, r.y + r.h / 2));  // no square where no icon is
    CHECK(at(top.x + top.w / 2, top.y + top.h / 2) != at(top.x + top.w / 2, top.y + top.h + 20));  // the thumb
}

// 3.0.11-beta.171 (the login concept: «Выбор сервера: пропускаем»): the server select with one row is chosen and
// confirmed at once - the game shows it even for one (uaRO HD's sclientinfo has one connection), after the login
// the char servers' list too; two rows and more wait for the player
TEST(pick_card_single_entry_goes_on_by_itself) {
    CHECK(pickcard::autoChoice(1, -1) == 0);
    CHECK(pickcard::autoChoice(1, 0) == 0);
    CHECK(pickcard::autoChoice(2, 0) == -1);
    CHECK(pickcard::autoChoice(0, -1) == -1);
}
