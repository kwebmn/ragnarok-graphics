#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/shop_live.hpp"

using namespace uagfx;
namespace sw = uagfx::shopwin;
namespace sl = uagfx::uikit::shop;

namespace {
panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }
float measure(const std::string& s, float px, bool bold) { return float(s.size()) * px * (bold ? 0.6f : 0.55f); }
std::string none(const panel::Picture*) { return {}; }
std::string glyph(int, float h, float& w) {
    w = h;
    return {};
}
}  // namespace

// 3.0.18-beta.99: the NPC's shop laid out by RmlUi - the core hit test on its boxes finds the tabs, the search, the
// list's rows (scrolled), the cart's − amount + ×, its row, the confirm and the clear
TEST(uikit_shop_refit_finds_wares_cart_and_buttons) {
    sw::Data d;
    d.name = "Tool Dealer";
    d.zeny = 100000;
    for (int i = 0; i < 14; ++i)
        d.wares.push_back(sw::Ware{501 + i, 50 + i, 10, 0, 0, true, "Ware " + std::to_string(i), nullptr});
    d.cart = {sw::Line{3, 5}, sw::Line{4, 1}};
    std::vector<int> shown;
    for (int i = 0; i < 14; ++i) shown.push_back(i);
    sw::Show sh;
    sh.scroll = 2;
    sw::Texts t;
    t.title = "Крамниця";
    t.buy = "Купити";
    t.sell = "Продати";
    t.search = "Пошук";
    t.ware = "Річ";
    t.price = "Ціна";
    t.weight = "Вага";
    t.cart = "Кошик";
    t.amount = "Кількість";
    t.sum = "Сума";
    t.total = "Разом";
    t.left = "Залишиться";
    t.weightBuy = "Вага після покупки";
    t.clear = "Очистити";
    t.note = "ПКМ — опис";
    t.space = "Пробіл";
    static sl::Model m;
    sl::fill(m, d, shown, sh, t, 1, none, glyph);
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(sl::rml(), [&](Rml::DataModelConstructor& c) { sl::bind(c, m); }, 200, 100, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    sw::Layout L = sw::layout(0, 0, 1, d, shown, sh, t, measure);
    CHECK(sl::refit(doc, L));
    CHECK(std::abs(float(L.w) - 1000) < 2.5f);
    CHECK_EQ(L.wares.size(), size_t(10));
    CHECK(std::abs(L.wares[1].y - L.wares[0].y - 35) < 0.5f);
    sw::Hit h = sw::hitTest(L, mid(L.wares[3]).x, mid(L.wares[3]).y);
    CHECK(h.part == sw::Part::Ware);
    CHECK_EQ(h.index, 5);  // the scroll counted
    h = sw::hitTest(L, mid(L.tabs[1]).x, mid(L.tabs[1]).y);
    CHECK(h.part == sw::Part::Tab);
    CHECK_EQ(h.index, 1);
    CHECK(sw::hitTest(L, mid(L.searchBox).x, mid(L.searchBox).y).part == sw::Part::Search);
    CHECK_EQ(L.lines.size(), size_t(2));
    CHECK(sw::hitTest(L, mid(L.less[1]).x, mid(L.less[1]).y).part == sw::Part::Less);
    h = sw::hitTest(L, mid(L.amounts[1]).x, mid(L.amounts[1]).y);
    CHECK(h.part == sw::Part::Amount);
    CHECK_EQ(h.index, 1);
    CHECK(sw::hitTest(L, mid(L.more[0]).x, mid(L.more[0]).y).part == sw::Part::More);
    CHECK(sw::hitTest(L, mid(L.drops[0]).x, mid(L.drops[0]).y).part == sw::Part::TakeBack);
    CHECK(sw::hitTest(L, L.lines[0].x + 60, mid(L.lines[0]).y).part == sw::Part::Line);
    // the cart's steps at core shopwin's places: − 240 px from the row's right (22 4 52 4 22 10 90 8 22 6)
    CHECK(std::abs(L.lines[0].x + L.lines[0].w - L.less[0].x - 240) < 4);
    CHECK(sw::hitTest(L, mid(L.confirm).x, mid(L.confirm).y).part == sw::Part::Confirm);
    CHECK(sw::hitTest(L, mid(L.clear).x, mid(L.clear).y).part == sw::Part::Clear);
    CHECK(L.cart.x > L.list.x + L.list.w);
    CHECK(L.footer.y >= L.list.y + L.list.h);
    rmlhost::close(doc);
}
