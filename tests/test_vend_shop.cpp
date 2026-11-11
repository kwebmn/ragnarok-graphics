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

// 3.0.18-beta.138 (the owner: a vending thing not seen into): a ware's slots
// in its name and its cards' line under it
TEST(vend_shop_slots_and_what_is_inside) {
    const auto name = [](int id) { return id == 4035 ? std::string("Hydra Card") : std::string(); };
    const int two[4] = {4035, 4035, 0, 0};
    CHECK_EQ(vendshop::withSlots("Gladius", 3), std::string("Gladius [3]"));
    CHECK_EQ(vendshop::withSlots("Jellopy", 0), std::string("Jellopy"));
    CHECK_EQ(vendshop::inside(two, 3, name, "{1} free"), std::string("Hydra Card \xC2\xB7 Hydra Card \xC2\xB7 1 free"));
    const int none[4] = {};
    CHECK_EQ(vendshop::inside(none, 2, name, "{1} free"), std::string("2 free"));
    CHECK_EQ(vendshop::inside(none, 0, name, "{1} free"), std::string());
    const int forged[4] = {255, 0, 0, 0};  // a forged thing's maker, not cards
    CHECK_EQ(vendshop::inside(forged, 0, name, "{1} free"), std::string());
    const int unknownCard[4] = {4999, 0, 0, 0};
    CHECK_EQ(vendshop::inside(unknownCard, 1, name, "{1} free"), std::string("#4999"));
}
