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

TEST(my_shop_words_and_picture) {
    std::vector<myshop::Row> r(1);
    r[0].name = "Red Potion";
    r[0].price = 1500;
    r[0].left = 12;
    const myshop::Layout L = myshop::layout(1920, 1080, 1.0f, r.size());
    myshop::Texts t;
    t.title = "Моя лавка: Cheap pots";
    t.left = "лишилось {1}";
    t.worth = "Ще на продаж";
    t.close = "Закрити лавку";
    t.empty = "Усе продано";
    const std::vector<myshop::Words> w = myshop::words(L, r, myshop::Show{}, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const myshop::Words& x) { return x.text == s; });
    };
    CHECK(has("Моя лавка: Cheap pots") && has("Red Potion") && has("1 500 z") && has("лишилось 12"));
    CHECK(has("18 000 z") && has("Закрити лавку"));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    myshop::paint(p, L, r, myshop::Show{});
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);
}
