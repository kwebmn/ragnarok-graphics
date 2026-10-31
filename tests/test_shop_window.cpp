// 3.0.11-beta.100 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's
// «Магазин»): the NPC's shop redrawn (core/shop_window) - the list on the left with a search, the cart on the right
// (− n + and each row's sum), the total, the zeny left (or after a sale) and the weight after; which shop of the
// reference pack the NPC is. 3.0.11-beta.101 (the owner: «чтобы в диалоге покупки работали также кнопки на клавиатуре
// верх вниз, также чтобы можно было легко вернуть выбраную на продажу вещь, чтобы если что можно было ввести
// количество с клавиатуры»): a row's most by the zeny and the weight left, the amount typed, a row taken back, a held
// button's steps, the keys in the footer
#include <string>
#include <vector>

#include "core/char_block.hpp"
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
    t.clear = "Очистити";
    t.note = "Ціни — від сервера, вага — з бази предметів";
    t.empty = "Клацніть товар, щоб додати";
    t.waiting = "Завантажую список…";
    t.choose = "вибір";
    t.toCart = "список / кошик";
    t.plus = "+1";
    t.minus = "−1";
    t.typeAmount = "кількість";
    t.remove = "прибрати";
    t.info = "опис";
    t.close = "Закрити";
    t.space = "Пробіл";
    t.editHint = "Enter — готово · Esc — скасувати · найбільше {1}";
    return t;
}

shopwin::Data data() {
    shopwin::Data d;
    d.wares.push_back(shopwin::Ware{501, 50, 70, 0, 0, true, "Red Potion", nullptr});
    d.wares.push_back(shopwin::Ware{1750, 1, 1, 0, 0, true, "Arrow", nullptr});
    d.wares.push_back(shopwin::Ware{504, 1200, 150, 0, 0, true, "White Potion", nullptr});
    d.wares.push_back(shopwin::Ware{611, 40, -1, 0, 0, true, "Magnifier", nullptr});  // its weight not known
    d.zeny = 1000000;
    d.weight = 1700;
    d.weightMost = 2430;
    return d;
}

}  // namespace

TEST(shop_window_a_rows_most_by_the_zeny_and_the_weight_left) {
    shopwin::Data d = data();
    CHECK_EQ(shopwin::mostFor(d, 0), (24300 - 17000) / 70);  // the weight left: 104 red potions
    d.zeny = 1000;
    CHECK_EQ(shopwin::mostFor(d, 0), 20);  // the zeny: 20 of 50 z
    d.zeny = 1000000;
    d.cart = {{2, 30}};  // 30 white potions: 36 000 z and 450 of the weight
    CHECK_EQ(shopwin::mostFor(d, 0), (24300 - 17000 - 4500) / 70);
    d.zeny = 10000000;
    CHECK_EQ(shopwin::mostFor(d, 3), shopwin::kMostBought);  // weight not known, money enough: the packet's most
    d.zeny = 1000000;
    CHECK_EQ(shopwin::mostFor(d, 3), (1000000 - 36000) / 40);  // the zeny left beside the white potions
    d.tab = shopwin::Tab::Sell;
    d.wares[1].count = 12;
    CHECK_EQ(shopwin::mostFor(d, 1), 12);  // sold: what the bag holds
    CHECK_EQ(shopwin::mostFor(d, 9), 0);
}

TEST(shop_window_cart_puts_takes_back_and_keeps_within_the_most) {
    shopwin::Data d = data();
    shopwin::put(d, 2, 1);
    shopwin::put(d, 0, 10);
    shopwin::put(d, 2, 29);
    CHECK(d.cart.size() == 2 && d.cart[0].ware == 2 && d.cart[0].amount == 30 && d.cart[1].ware == 0);  // put-in order
    shopwin::put(d, 0, 99999);
    CHECK_EQ(shopwin::amountOf(d.cart, 0), shopwin::mostFor(d, 0));
    shopwin::put(d, 2, -30);  // at 0 the row leaves the cart
    CHECK(d.cart.size() == 1 && shopwin::amountOf(d.cart, 2) == 0);
    shopwin::put(d, 3, -1);  // nothing to take away
    CHECK_EQ(d.cart.size(), 1u);
    shopwin::takeBack(d, 0);  // the whole row back
    CHECK(d.cart.empty());
    d.zeny = 30;
    shopwin::put(d, 0, 1);  // not money enough for one
    CHECK(d.cart.empty());
}

TEST(shop_window_the_amount_typed) {
    shopwin::Data d = data();
    d.zeny = 1000;
    shopwin::Edit e;
    shopwin::typeDigit(d, e, 0, '2');  // a ware not in the cart: its row with the amount typed
    CHECK(e.ware == 0 && e.digits == "2" && shopwin::amountOf(d.cart, 0) == 2);
    shopwin::typeDigit(d, e, 0, '5');  // 25 - more than 1000 z buys: the most
    CHECK(e.digits == "20" && shopwin::amountOf(d.cart, 0) == 20);
    shopwin::eraseDigit(d, e);
    CHECK(e.digits == "2" && shopwin::amountOf(d.cart, 0) == 2);
    shopwin::cancelEdit(d, e);  // Esc: as before the typing - it was not in the cart
    CHECK(e.ware == -1 && d.cart.empty());
    shopwin::put(d, 1, 5);
    shopwin::typeDigit(d, e, 1, '3');  // the first digit replaces the amount
    CHECK(shopwin::amountOf(d.cart, 1) == 3 && e.before == 5);
    shopwin::eraseDigit(d, e);  // emptied: kept while typing
    CHECK(d.cart.size() == 1 && shopwin::amountOf(d.cart, 1) == 0);
    shopwin::typeDigit(d, e, 1, '7');
    shopwin::endEdit(d, e);  // Enter: as typed
    CHECK(e.ware == -1 && shopwin::amountOf(d.cart, 1) == 7);
    shopwin::typeDigit(d, e, 1, '0');
    shopwin::endEdit(d, e);  // 0: the row leaves
    CHECK(d.cart.empty());
    shopwin::typeDigit(d, e, 1, '4');
    shopwin::typeDigit(d, e, 0, '1');  // another ware: the first one's typing ends as it was
    CHECK(shopwin::amountOf(d.cart, 1) == 4 && shopwin::amountOf(d.cart, 0) == 1 && e.ware == 0);
}

TEST(shop_window_a_held_button_steps) {
    shopwin::HoldRepeat r;
    CHECK_EQ(r.step(true, 1000), 1);  // the press
    CHECK_EQ(r.step(true, 1100), 0);
    CHECK_EQ(r.step(true, 1400), 1);  // held 0.4 s: one every 80 ms
    CHECK_EQ(r.step(true, 1450), 0);
    CHECK_EQ(r.step(true, 1480), 1);
    CHECK_EQ(r.step(true, 2500), 10);  // held 1.5 s: ten at a time
    CHECK_EQ(r.step(false, 2600), 0);
    CHECK_EQ(r.step(true, 2700), 1);  // a new press
}

TEST(shop_window_sums_money_and_weight) {
    const shopwin::Data base = data();
    std::vector<shopwin::Line> cart{{2, 30}, {1, 50}, {3, 2}};
    const shopwin::Sums b = shopwin::sums(shopwin::Tab::Buy, cart, base.wares, 1482300, 1700, 2430);
    CHECK_EQ(b.total, 36000 + 50 + 80);
    CHECK_EQ(b.zenyAfter, 1482300 - 36130);
    CHECK_EQ(b.weightAfter, 1700 + 450 + 5);  // 30 x 15 + 50 x 0.1; the magnifier's weight not known
    CHECK(b.ok && b.weightPercent == (1700 + 455) * 100 / 2430);
    CHECK(!shopwin::sums(shopwin::Tab::Buy, cart, base.wares, 36129, 1700, 2430).ok);   // not money enough
    CHECK(!shopwin::sums(shopwin::Tab::Buy, cart, base.wares, 1482300, 2000, 2430).ok);  // past the weight
    CHECK(!shopwin::sums(shopwin::Tab::Buy, {}, base.wares, 1482300, 1700, 2430).ok);    // nothing in it
    std::vector<shopwin::Ware> bag = base.wares;
    bag[0].count = 20;
    const shopwin::Sums s = shopwin::sums(shopwin::Tab::Sell, {{0, 20}}, bag, 1000, 300, 2430);
    CHECK(s.total == 1000 && s.zenyAfter == 2000 && s.weightAfter == 300 - 140 && s.ok);
    CHECK(shopwin::weightText(70) == "7" && shopwin::weightText(1) == "0.1" && shopwin::weightText(15) == "1.5");
    CHECK(shopwin::money(36000) == "36\xC2\xA0" "000 z");
}

TEST(shop_window_search_finds_by_name) {
    const shopwin::Data d = data();
    CHECK_EQ(shopwin::shownOf(d.wares, "").size(), 4u);
    const std::vector<int> pots = shopwin::shownOf(d.wares, "POT");
    CHECK(pots.size() == 2 && pots[0] == 0 && pots[1] == 2);
    CHECK(shopwin::shownOf(d.wares, "wing").empty());
}

TEST(shop_window_layout_list_left_cart_right) {
    shopwin::Data d = data();
    d.name = "Tool Dealer";
    d.place = "prontera";
    d.cart = {{2, 30}, {1, 50}};
    d.zeny = 1482300;
    const std::vector<int> shown = shopwin::shownOf(d.wares, "");
    shopwin::Show sh;
    const shopwin::Layout L = shopwin::layout(0, 0, 1.0f, d, shown, sh, texts(), measure);
    CHECK(L.tabs.size() == 2 && L.wares.size() == 4 && L.lines.size() == 2);
    CHECK(L.list.x + L.list.w < L.cart.x);  // the list on the left, the cart on the right
    CHECK(L.confirm.w > 0 && L.confirm.y + L.confirm.h <= L.glass.h && L.clear.x > L.confirm.x);
    CHECK(L.confirmKey.w > 0 && L.confirmKey.x > L.confirm.x && L.confirmKey.x + L.confirmKey.w <= L.confirm.x +
                                                                                                      L.confirm.w);
    CHECK(L.less.size() == 2 && L.more.size() == 2 && L.amounts.size() == 2 && L.drops.size() == 2);
    CHECK(L.less[0].x < L.amounts[0].x && L.amounts[0].x < L.more[0].x && L.more[0].x < L.drops[0].x);
    CHECK(L.keyCaps.size() == 7 && L.keyWords.size() == 7);  // the keys in the footer
    const panel::Box& row = L.wares[2];
    shopwin::Hit h = shopwin::hitTest(L, row.x + 4, row.y + row.h / 2);
    CHECK(h.part == shopwin::Part::Ware && h.index == 2);
    h = shopwin::hitTest(L, L.more[1].x + 2, L.more[1].y + 2);
    CHECK(h.part == shopwin::Part::More && h.index == 1);
    h = shopwin::hitTest(L, L.drops[1].x + 2, L.drops[1].y + 2);
    CHECK(h.part == shopwin::Part::TakeBack && h.index == 1);
    h = shopwin::hitTest(L, L.amounts[0].x + 2, L.amounts[0].y + 2);
    CHECK(h.part == shopwin::Part::Amount && h.index == 0);
    h = shopwin::hitTest(L, L.tabs[1].x + 2, L.tabs[1].y + 2);
    CHECK(h.part == shopwin::Part::Tab && h.index == 1);
    CHECK(shopwin::hitTest(L, L.searchBox.x + 40, L.searchBox.y + 4).part == shopwin::Part::Search);
    CHECK(shopwin::hitTest(L, L.confirm.x + 4, L.confirm.y + 4).part == shopwin::Part::Confirm);
    sh.scroll = 1;  // scrolled: the row under the mouse is one further down the list
    const shopwin::Layout S = shopwin::layout(0, 0, 1.0f, d, shown, sh, texts(), measure);
    CHECK(S.wares.size() == 3 && shopwin::hitTest(S, S.wares[0].x + 4, S.wares[0].y + 4).index == 1);
    sh.pad = true;  // the controller's buttons in the footer and its bumpers at the tabs
    const shopwin::Layout P = shopwin::layout(0, 0, 1.0f, d, shown, sh, texts(), measure);
    CHECK(P.padGlyphs.size() == 5 && P.padWords.size() == 5 && P.tabGlyphs[0].w > 0 && P.tabGlyphs[1].w > 0);
    CHECK(P.keyCaps.empty() && P.confirmKey.w > 0);
    std::vector<shopwin::Words> w = shopwin::words(P, d, shown, sh, texts(), measure);
    bool total = false, title = false;
    for (const shopwin::Words& x : w) {
        total = total || x.text == "36\xC2\xA0" "050 z";
        title = title || x.text == "Tool Dealer";
    }
    CHECK(total && title);
    sh.pad = false;
    shopwin::Edit e;  // typing an amount: its digits in the row, the hint with the most under the cart
    shopwin::typeDigit(d, e, 1, '4');
    sh.edit = e;
    const shopwin::Layout E = shopwin::layout(0, 0, 1.0f, d, shown, sh, texts(), measure);
    w = shopwin::words(E, d, shown, sh, texts(), measure);
    bool digits = false, hint = false;
    for (const shopwin::Words& x : w) {
        digits = digits || x.text == "4";
        hint = hint || x.text.find(charblock::grouped(shopwin::mostFor(d, 1))) != std::string::npos;
    }
    CHECK(digits && hint);
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
