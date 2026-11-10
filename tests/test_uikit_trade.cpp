#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/trade_live.hpp"

using namespace uagfx;
namespace tw = uagfx::tradewin;
namespace tl = uagfx::uikit::trade;

namespace {

tw::Texts texts() {
    tw::Texts t;
    t.title = "Обмін";
    t.partner = "Partner";
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

panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }

}  // namespace

// 3.0.18-beta.95: «Обмін» laid out by RmlUi; the core hit test on its boxes finds both sides' cells, the zeny field,
// the buttons, × and the title; the window keeps its height with or without the warning
TEST(uikit_trade_refit_finds_cells_field_and_buttons) {
    tw::Side me, them;
    me.slots[0] = tw::Thing{501, 3, 0, true, "Red Potion", nullptr};
    them.slots[6] = tw::Thing{1201, 1, 7, true, "Knife", nullptr};
    me.zeny = 1500;
    tw::Show sh;
    static tl::Model m;
    tl::fill(m, me, them, sh, texts(), 1, [](const panel::Picture*) { return std::string(); });
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(tl::rml(), [&](Rml::DataModelConstructor& c) { tl::bind(c, m); }, 300, 200, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    tw::Layout L = tw::layout(0, 0, 1);
    CHECK(tl::refit(d, L));
    CHECK(std::abs(float(L.w) - 520) < 2.5f);
    CHECK(std::abs(L.cells[1].x - L.cells[0].x - 48) < 0.5f);  // five a row
    CHECK(std::abs(L.cells[5].y - L.cells[0].y - 48) < 0.5f);
    CHECK(std::abs(L.cells[5].x - L.cells[0].x) < 0.5f);
    CHECK(L.cells[10].x > L.cells[4].x + L.cells[4].w);  // the partner's side on the right
    CHECK(std::abs(L.cells[10].y - L.cells[0].y) < 0.5f);
    for (int i : {0, 4, 9, 10, 16, 19}) {
        const panel::Box p = mid(L.cells[i]);
        const tw::Hit h = tw::hitTest(L, p.x, p.y);
        CHECK(h.part == tw::Part::Cell);
        CHECK_EQ(h.index, i);
    }
    panel::Box p = mid(L.zenyField);
    CHECK(tw::hitTest(L, p.x, p.y).part == tw::Part::Zeny);
    for (int i = 0; i < 3; ++i) {
        p = mid(L.buttons[i]);
        const tw::Hit h = tw::hitTest(L, p.x, p.y);
        CHECK(h.part == tw::Part::Button);
        CHECK_EQ(h.index, i);
    }
    p = mid(L.close);
    CHECK(tw::hitTest(L, p.x, p.y).part == tw::Part::Close);
    CHECK(tw::hitTest(L, 40, 20).part == tw::Part::Title);
    const int height = L.h;
    sh.late.zeny = true;  // the warning shown: the same height (its row kept, unseen before)
    tl::fill(m, me, them, sh, texts(), 1, [](const panel::Picture*) { return std::string(); });
    rmlhost::update();
    CHECK(tl::refit(d, L));
    CHECK_EQ(L.h, height);
    CHECK_EQ(m.warn, std::string("Змінено після вашого «Готово»"));
    rmlhost::close(d);
}
