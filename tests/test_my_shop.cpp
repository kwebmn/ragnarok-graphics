// 3.0.11-beta.166 (task 4 of the remaining windows, the merchant's: one's own open shop): the game's own-shop window
// (UI window 0x2D) as a card (core/my_shop) - the shop's sign, a row a thing still for sale (its price, how many are
// left), its takings so far, «Закрити лавку» the window's own close (0x87: the game closes the shop)
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/my_shop.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

TEST(my_shop_the_windows_close) { CHECK(myshop::kCloseButton == 0x87); }

// What is still for sale is worth: each left times its price
TEST(my_shop_worth_left) {
    std::vector<myshop::Row> r(2);
    r[0].price = 100;
    r[0].left = 5;
    r[1].price = 2500;
    r[1].left = 2;
    CHECK(myshop::worth(r) == 5500);
}

TEST(my_shop_places_and_the_mouse) {
    const myshop::Layout L = myshop::layout(1920, 1080, 1.0f, 3);
    CHECK(L.rows.size() == 3 && L.close.y > L.rows[2].y);
    using P = myshop::Part;
    CHECK(myshop::hitTest(L, mid(L.close).first, mid(L.close).second).part == P::Close);
    CHECK(myshop::hitTest(L, mid(L.cross).first, mid(L.cross).second).part == P::Close);
    CHECK(myshop::hitTest(L, -5, -5).part == P::None);
    const myshop::Layout many = myshop::layout(1920, 1080, 1.0f, 20);
    CHECK(many.rows.size() == 8 && myshop::mostScroll(many, 20) == 12);
}

// 3.0.18-beta.30: uaUI's own shop - a row's icon square inside its row
TEST(my_shop_icon_inside_its_row) {
    const panel::Box row{10, 20, 300, 40};
    const panel::Box b = myshop::iconOf(row, 1.0f);
    CHECK(b.w > 0 && b.x > row.x && b.y >= row.y && b.y + b.h <= row.y + row.h);
}

// 3.0.18-beta.143 (the windows' audit: «Моя лавка» showed nothing of what sold): the sales as the server tells them
// (0x0137: a place, how many) matched to the shop's things by their place - each sale's name and zeny, how many of
// each sold, the zeny earned; a place not known: no sale
TEST(my_shop_sales_ledger) {
    myshop::Ledger l;
    l.stock(2, "Arrow of Wind", 3);
    l.stock(5, "Jellopy", 10);
    myshop::Sale sale;
    CHECK(l.sold(2, 100, "10:42", sale));
    CHECK_EQ(sale.name, std::string("Arrow of Wind"));
    CHECK_EQ(sale.amount, 100);
    CHECK_EQ(sale.zeny, 300LL);
    CHECK_EQ(sale.time, std::string("10:42"));
    CHECK(l.sold(2, 5, "10:43", sale));
    CHECK_EQ(l.soldOf(2), 105);
    CHECK_EQ(l.soldOf(5), 0);
    CHECK_EQ(l.earned(), 315LL);
    CHECK(!l.sold(9, 1, "10:44", sale));
    CHECK_EQ(l.sales().size(), size_t(2));
    CHECK_EQ(l.sales().front().amount, 5);  // the newest first
    l.stock(2, "Arrow of Wind", 3);  // the list read again keeps the counts
    CHECK_EQ(l.soldOf(2), 105);
    l.clear();
    CHECK_EQ(l.earned(), 0LL);
    CHECK(l.sales().empty());
}
