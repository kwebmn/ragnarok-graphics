// 3.0.11-beta.156 (task 4 of the remaining windows, the merchant's: buying from another's shop): «Лавка» - the shop's
// things with their prices and how many it has, the purchase chosen in the same rows (the game's own purchase list),
// its total against the zeny, the buttons; the zeny written with spaces
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/vend_shop.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::vector<vendshop::Row> rows() {
    std::vector<vendshop::Row> r(3);
    r[0].index = 2;
    r[0].id = 501;
    r[0].name = "Red Potion";
    r[0].price = 50;
    r[0].stock = 100;
    r[0].chosen = 10;
    r[1].index = 3;
    r[1].id = 1201;
    r[1].name = "Knife";
    r[1].refine = 7;
    r[1].price = 1500000;
    r[1].stock = 1;
    r[2].index = 4;
    r[2].id = 4001;
    r[2].name = "Poring Card";
    r[2].price = 9000;
    r[2].stock = 2;
    r[2].chosen = 2;
    return r;
}

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

TEST(vend_shop_zeny_with_spaces) {
    CHECK(vendshop::money(0) == "0");
    CHECK(vendshop::money(950) == "950");
    CHECK(vendshop::money(1500000) == "1 500 000");
    CHECK(vendshop::money(12345) == "12 345");
}

// The purchase's total (each chosen times its price); one more of a thing: while the shop has more of it and the zeny
// pays for it (the game's own check: price x count + the total over the zeny refused)
TEST(vend_shop_total_and_one_more) {
    const std::vector<vendshop::Row> r = rows();
    CHECK(vendshop::total(r) == 10 * 50 + 2 * 9000);
    CHECK(vendshop::canAdd(r[0], vendshop::total(r), 1000000));
    CHECK(!vendshop::canAdd(r[2], vendshop::total(r), 1000000));  // the shop's two both chosen
    CHECK(!vendshop::canAdd(r[1], vendshop::total(r), 1000000));  // 1 500 000 over the zeny
    CHECK(vendshop::canAdd(r[1], vendshop::total(r), 2000000));
    CHECK(vendshop::canRemove(r[0]) && !vendshop::canRemove(r[1]));
}

// A card at the middle: a row a thing (as many as fit, 8 at most, the wheel for the rest), each row's − and + at its
// right end, the total and the zeny under them, «Купити» and «Скасувати»
TEST(vend_shop_places_and_the_mouse) {
    const vendshop::Layout L = vendshop::layout(1920, 1080, 1.0f, 3);
    CHECK(L.lines.size() == 3 && L.plus.size() == 3 && L.minus.size() == 3);
    CHECK(L.x + L.w / 2 > 900 && L.x + L.w / 2 < 1020);
    CHECK(L.plus[0].x > L.minus[0].x && L.plus[0].x + L.plus[0].w <= L.lines[0].x + L.lines[0].w + 1);
    CHECK(L.buy.y > L.lines[2].y && L.cancel.y == L.buy.y);
    using P = vendshop::Part;
    const vendshop::Hit plus = vendshop::hitTest(L, 0, 3, mid(L.plus[1]).first, mid(L.plus[1]).second);
    CHECK(plus.part == P::Plus && plus.row == 1);
    const vendshop::Hit minus = vendshop::hitTest(L, 0, 3, mid(L.minus[2]).first, mid(L.minus[2]).second);
    CHECK(minus.part == P::Minus && minus.row == 2);
    CHECK(vendshop::hitTest(L, 0, 3, mid(L.buy).first, mid(L.buy).second).part == P::Buy);
    CHECK(vendshop::hitTest(L, 0, 3, mid(L.cancel).first, mid(L.cancel).second).part == P::Cancel);
    CHECK(vendshop::hitTest(L, 0, 3, -5, -5).part == P::None);
    const vendshop::Layout many = vendshop::layout(1920, 1080, 1.0f, 20);
    CHECK(many.lines.size() == 8 && vendshop::mostScroll(many, 20) == 12);
    const vendshop::Hit scrolled = vendshop::hitTest(many, 5, 20, mid(many.plus[0]).first, mid(many.plus[0]).second);
    CHECK(scrolled.part == P::Plus && scrolled.row == 5);
}

// Its words: the shop's title, each thing's name (+7 shown), its price, how many the shop has, the chosen count, the
// total and the zeny (the total red when it is more than the zeny)
TEST(vend_shop_words_and_picture) {
    const std::vector<vendshop::Row> r = rows();
    const vendshop::Layout L = vendshop::layout(1920, 1080, 1.0f, r.size());
    vendshop::Texts t;
    t.title = "Лавка";
    t.stock = "є {1}";
    t.total = "Разом";
    t.zeny = "Ваші зені";
    t.buy = "Купити";
    t.cancel = "Скасувати";
    vendshop::Show sh;
    sh.zeny = 100;
    const std::vector<vendshop::Words> w = vendshop::words(L, r, sh, t);
    const auto find = [&](const std::string& s) {
        return std::find_if(w.begin(), w.end(), [&](const vendshop::Words& x) { return x.text == s; });
    };
    CHECK(find("Лавка") != w.end() && find("Red Potion") != w.end() && find("+7 Knife") != w.end());
    CHECK(find("1 500 000 z") != w.end() && find("є 100") != w.end() && find("10") != w.end());
    CHECK(find("18 500 z") != w.end() && find("100 z") != w.end());
    CHECK(find("18 500 z") != w.end() && find("18 500 z")->rgb == vendshop::kShort);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    vendshop::paint(p, L, r, sh);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);
}

// 3.0.18-beta.30: uaUI's «Лавка» - a line's icon square inside its line
TEST(vend_shop_icon_inside_its_line) {
    const vendshop::Layout L = vendshop::layout(1920, 1080, 1.0f, 3);
    const panel::Box b = vendshop::iconAt(L, 1);
    CHECK(b.w > 0 && b.x > L.lines[1].x && b.y >= L.lines[1].y && b.y + b.h <= L.lines[1].y + L.lines[1].h);
    CHECK(vendshop::iconAt(L, 9).w == 0);
}
