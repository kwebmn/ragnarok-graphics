// 3.0.11-beta.100 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's
// «Магазин»): the NPC's shop redrawn (core/shop_window) - the list on the left with a search, the cart on the right
// (− n + and each row's sum), the total, the zeny left (or after a sale) and the weight after; which shop of the
// reference pack the NPC is
#include <string>
#include <vector>

#include "core/reference_index.hpp"
#include "core/shop_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

float measure(const std::string& t, float px, bool bold) { return float(t.size()) * px * (bold ? 0.6f : 0.55f); }

shopwin::Texts texts() {
    shopwin::Texts t;
    t.title = "Крамниця";
    t.buy = "Купити";
    t.sell = "Продати";
    t.search = "Пошук у крамниці";
    t.ware = "Товар";
    t.price = "Ціна";
    t.weight = "Вага";
    t.cart = "Кошик";
    t.amount = "Кількість";
    t.sum = "Сума";
    t.total = "Разом";
    t.left = "Залишиться";
    t.after = "Буде";
    t.weightBuy = "Вага після покупки";
    t.weightSell = "Вага після продажу";
    t.buyButton = "Купити · Enter";
    t.sellButton = "Продати · Enter";
    t.clear = "Очистити";
    t.note = "Ціни — від сервера, вага — з бази предметів";
    t.empty = "Клацніть товар, щоб додати";
    t.mouse = "Клацання +1 · Shift +10 · права кнопка −1";
    t.waiting = "Завантаження…";
    t.plus = "+1";
    t.minus = "−1";
    t.close = "Закрити";
    return t;
}

std::vector<shopwin::Ware> wares() {
    std::vector<shopwin::Ware> w(4);
    w[0] = shopwin::Ware{501, 50, 70, 0, 0, true, "Red Potion", nullptr};
    w[1] = shopwin::Ware{1750, 1, 1, 0, 0, true, "Arrow", nullptr};
    w[2] = shopwin::Ware{504, 1200, 150, 0, 0, true, "White Potion", nullptr};
    w[3] = shopwin::Ware{611, 40, -1, 0, 0, true, "Magnifier", nullptr};  // its weight not known
    return w;
}

}  // namespace

TEST(shop_window_cart_puts_and_keeps_within_the_most) {
    const std::vector<shopwin::Ware> w = wares();
    std::vector<shopwin::Line> cart;
    shopwin::put(cart, w, 2, 1, shopwin::Tab::Buy);
    shopwin::put(cart, w, 0, 10, shopwin::Tab::Buy);
    shopwin::put(cart, w, 2, 29, shopwin::Tab::Buy);
    CHECK(cart.size() == 2 && cart[0].ware == 2 && cart[0].amount == 30 && cart[1].ware == 0);  // put-in order
    shopwin::put(cart, w, 0, 99999, shopwin::Tab::Buy);
    CHECK_EQ(shopwin::amountOf(cart, 0), shopwin::kMostBought);
    shopwin::put(cart, w, 2, -30, shopwin::Tab::Buy);  // at 0 the row leaves the cart
    CHECK(cart.size() == 1 && shopwin::amountOf(cart, 2) == 0);
    shopwin::put(cart, w, 3, -1, shopwin::Tab::Buy);  // nothing to take away
    CHECK_EQ(cart.size(), 1u);
    std::vector<shopwin::Ware> bag = w;  // sold: no more than the bag holds
    bag[1].count = 12;
    std::vector<shopwin::Line> sold;
    shopwin::put(sold, bag, 1, 50, shopwin::Tab::Sell);
    CHECK_EQ(shopwin::amountOf(sold, 1), 12);
    shopwin::put(sold, bag, 0, 1, shopwin::Tab::Sell);  // none of it in the bag
    CHECK_EQ(sold.size(), 1u);
    shopwin::put(sold, bag, 9, 1, shopwin::Tab::Sell);  // no such row
    CHECK_EQ(sold.size(), 1u);
}

TEST(shop_window_sums_money_and_weight) {
    const std::vector<shopwin::Ware> w = wares();
    std::vector<shopwin::Line> cart{{2, 30}, {1, 50}, {3, 2}};
    const shopwin::Sums b = shopwin::sums(shopwin::Tab::Buy, cart, w, 1482300, 1700, 2430);
    CHECK_EQ(b.total, 36000 + 50 + 80);
    CHECK_EQ(b.zenyAfter, 1482300 - 36130);
    CHECK_EQ(b.weightAfter, 1700 + 450 + 5);  // 30 x 15 + 50 x 0.1; the magnifier's weight not known
    CHECK(b.ok && b.weightPercent == (1700 + 455) * 100 / 2430);
    CHECK(!shopwin::sums(shopwin::Tab::Buy, cart, w, 36129, 1700, 2430).ok);  // not money enough
    CHECK(!shopwin::sums(shopwin::Tab::Buy, cart, w, 1482300, 2000, 2430).ok);  // past the weight
    CHECK(!shopwin::sums(shopwin::Tab::Buy, {}, w, 1482300, 1700, 2430).ok);  // nothing in it
    std::vector<shopwin::Ware> bag = w;
    bag[0].count = 20;
    const shopwin::Sums s = shopwin::sums(shopwin::Tab::Sell, {{0, 20}}, bag, 1000, 300, 2430);
    CHECK(s.total == 1000 && s.zenyAfter == 2000 && s.weightAfter == 300 - 140 && s.ok);
    CHECK(shopwin::weightText(70) == "7" && shopwin::weightText(1) == "0.1" && shopwin::weightText(15) == "1.5");
    CHECK(shopwin::money(36000) == "36\xC2\xA0" "000 z");
}

TEST(shop_window_search_finds_by_name) {
    const std::vector<shopwin::Ware> w = wares();
    CHECK_EQ(shopwin::shownOf(w, "").size(), 4u);
    const std::vector<int> pots = shopwin::shownOf(w, "POT");
    CHECK(pots.size() == 2 && pots[0] == 0 && pots[1] == 2);
    CHECK(shopwin::shownOf(w, "wing").empty());
}

TEST(shop_window_layout_list_left_cart_right) {
    shopwin::Data d;
    d.name = "Tool Dealer";
    d.place = "prontera";
    d.wares = wares();
    d.cart = {{2, 30}, {1, 50}};
    d.zeny = 1482300;
    d.weight = 1700;
    d.weightMost = 2430;
    const std::vector<int> shown = shopwin::shownOf(d.wares, "");
    shopwin::Show sh;
    const shopwin::Layout L = shopwin::layout(0, 0, 1.0f, d, shown, sh, texts(), measure);
    CHECK(L.tabs.size() == 2 && L.wares.size() == 4 && L.lines.size() == 2);
    CHECK(L.list.x + L.list.w < L.cart.x);  // the list on the left, the cart on the right
    CHECK(L.confirm.w > 0 && L.confirm.y + L.confirm.h <= L.glass.h && L.clear.x > L.confirm.x);
    CHECK(L.less.size() == 2 && L.more.size() == 2 && L.less[0].x < L.more[0].x);
    const panel::Box& row = L.wares[2];
    shopwin::Hit h = shopwin::hitTest(L, row.x + 4, row.y + row.h / 2);
    CHECK(h.part == shopwin::Part::Ware && h.index == 2);
    h = shopwin::hitTest(L, L.more[1].x + 2, L.more[1].y + 2);
    CHECK(h.part == shopwin::Part::More && h.index == 1);
    h = shopwin::hitTest(L, L.tabs[1].x + 2, L.tabs[1].y + 2);
    CHECK(h.part == shopwin::Part::Tab && h.index == 1);
    CHECK(shopwin::hitTest(L, L.searchBox.x + 40, L.searchBox.y + 4).part == shopwin::Part::Search);
    CHECK(shopwin::hitTest(L, L.confirm.x + 4, L.confirm.y + 4).part == shopwin::Part::Confirm);
    sh.scroll = 1;  // scrolled: the row under the mouse is one further down the list
    const shopwin::Layout S = shopwin::layout(0, 0, 1.0f, d, shown, sh, texts(), measure);
    CHECK(S.wares.size() == 3 && shopwin::hitTest(S, S.wares[0].x + 4, S.wares[0].y + 4).index == 1);
    sh.pad = true;  // the controller's buttons in the footer and its bumpers at the tabs
    const shopwin::Layout P = shopwin::layout(0, 0, 1.0f, d, shown, sh, texts(), measure);
    CHECK(P.padGlyphs.size() == 4 && P.padWords.size() == 4 && P.tabGlyphs[0].w > 0 && P.tabGlyphs[1].w > 0);
    std::vector<shopwin::Words> w = shopwin::words(P, d, shown, sh, texts(), measure);
    bool total = false, title = false;
    for (const shopwin::Words& x : w) {
        total = total || x.text == "36\xC2\xA0" "050 z";
        title = title || x.text == "Tool Dealer";
    }
    CHECK(total && title);
}

TEST(shop_window_scroll_keeps_the_frame_in_sight) {
    CHECK_EQ(shopwin::scrollFor(0, 3, 10, 40), 0);
    CHECK_EQ(shopwin::scrollFor(15, 0, 10, 40), 6);
    CHECK_EQ(shopwin::scrollFor(12, 5, 10, 40), 5);
    CHECK_EQ(shopwin::scrollFor(2, 7, 10, 8), 0);  // fewer than the rows: no scroll
}

TEST(shop_window_which_shop_the_npc_is) {
    std::vector<refindex::Shop> shops;
    shops.push_back(refindex::Shop{"alberta_in", "Tool Dealer", 182, 97, {{501, 50}, {502, 200}, {1750, 1}}});
    shops.push_back(refindex::Shop{"alberta_in", "Armor Dealer", 180, 15, {{2301, 10}, {2401, 400}}});
    shops.push_back(refindex::Shop{"prontera", "Tool Dealer", 134, 221, {{501, 50}}});
    CHECK_EQ(shopwin::matchShop(shops, "alberta_in", {2301, 2401}, 182, 95), 1);  // by its wares, not the nearest
    CHECK_EQ(shopwin::matchShop(shops, "alberta_in", {}, 181, 20), 1);  // a sale's list: the nearest
    CHECK_EQ(shopwin::matchShop(shops, "alberta_in", {}, 100, 60), -1);  // none near
    CHECK_EQ(shopwin::matchShop(shops, "prontera", {9999}, 134, 220), 2);  // none of its wares: the nearest one
    CHECK_EQ(shopwin::matchShop(shops, "payon", {501}, 134, 220), -1);  // not on this map
}
