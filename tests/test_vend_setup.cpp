// 3.0.16-beta.33 (the owner: «делай» - the shop's opening in the new style): the game's vending windows 0x29 and 0x2A
// as one card - the cart's things and the sale's, a price field each, the game's own OK and cancel
#include <string>
#include <utility>
#include <vector>

#include "core/vend_setup.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

TEST(vend_setup_prices) {
    CHECK_EQ(vendsetup::priceOf("1500"), 1500LL);
    CHECK_EQ(vendsetup::priceOf(""), 0LL);
    CHECK_EQ(vendsetup::priceOf("1 250 000"), 1250000LL);
    CHECK(vendsetup::priceOf("9999999999999") > 99990000LL);  // past the game's most: its OK asks
}

TEST(vend_setup_card_parts) {
    const vendsetup::Layout L = vendsetup::layout(1920, 1080, 1.0f);
    CHECK(L.cartRows.size() == L.saleRows.size() && L.prices.size() == L.saleRows.size());
    CHECK(L.cartList.x < L.saleList.x);
    auto [ox, oy] = mid(L.open);
    CHECK(vendsetup::hitTest(L, ox, oy).part == vendsetup::Part::Open);
    auto [cx, cy] = mid(L.cancel);
    CHECK(vendsetup::hitTest(L, cx, cy).part == vendsetup::Part::Cancel);
    auto [sx, sy] = mid(L.sign);
    CHECK(vendsetup::hitTest(L, sx, sy).part == vendsetup::Part::Sign);
    auto [px, py] = mid(L.prices[2]);
    CHECK(vendsetup::hitTest(L, px, py).part == vendsetup::Part::Price && vendsetup::hitTest(L, px, py).index == 2);
    auto [rx, ry] = mid(L.removes[1]);
    CHECK(vendsetup::hitTest(L, rx, ry).part == vendsetup::Part::Remove);
    CHECK(vendsetup::hitTest(L, L.cartRows[3].x + 10, L.cartRows[3].y + 10).part == vendsetup::Part::Cart);
    CHECK(L.removes[0].x + L.removes[0].w <= L.saleRows[0].x + L.saleRows[0].w);
    CHECK_EQ(vendsetup::mostScroll(L, false, 3), 0);
    CHECK_EQ(vendsetup::mostScroll(L, true, 12), 12 - int(L.saleRows.size()));
}

TEST(vend_setup_words) {
    const vendsetup::Layout L = vendsetup::layout(1920, 1080, 1.0f);
    std::vector<vendsetup::Row> cart(1), sale(1);
    cart[0].name = "Red Potion";
    cart[0].count = 20;
    sale[0].name = "Knife";
    sale[0].price = "1500";
    vendsetup::Show sh;
    sh.most = 4;
    vendsetup::Texts t;
    t.sale = "For sale {1} / {2}";
    bool head = false, potion = false, price = false;
    for (const vendsetup::Words& w : vendsetup::words(L, cart, sale, "", sh, t)) {
        head = head || w.text == "For sale 1 / 4";
        potion = potion || w.text == "Red Potion \xC3\x97" "20";
        price = price || w.text == "1 500";
    }
    CHECK(head && potion && price);
}

// 3.0.16-beta.39: the pad's price steps - a power of ten 1-10 % of it, rounded, 10 from nothing, the game's most
TEST(vend_setup_price_steps) {
    CHECK(vendsetup::stepPrice("", 1) == "10");
    CHECK(vendsetup::stepPrice("", -1) == "0");
    CHECK(vendsetup::stepPrice("50", 1) == "51");
    CHECK(vendsetup::stepPrice("1000", 1) == "1100");
    CHECK(vendsetup::stepPrice("1050", 1) == "1100");
    CHECK(vendsetup::stepPrice("10000", -1) == "9900");
    CHECK(vendsetup::stepPrice("1 250 000", -1) == "1200000");
    CHECK(vendsetup::stepPrice("99990000", 1) == "99990000");
    CHECK(vendsetup::stepPrice("1", -1) == "0");
}

// 3.0.18-beta.30: uaUI's shop opening - a row's icon square inside its row
TEST(vend_setup_icon_inside_its_row) {
    const panel::Box row{10, 20, 300, 40};
    const panel::Box b = vendsetup::iconOf(row, 1.0f);
    CHECK(b.w > 0 && b.x > row.x && b.y >= row.y && b.y + b.h <= row.y + row.h);
}
