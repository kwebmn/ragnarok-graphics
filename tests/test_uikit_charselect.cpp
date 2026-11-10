#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/charselect_live.hpp"

using namespace uagfx;
namespace cs = uagfx::charselect;
namespace cl = uagfx::uikit::charpick;

namespace {
panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }
cs::Texts texts() {
    cs::Texts t;
    t.title = "Вибір персонажа";
    t.enter = "Увійти";
    t.make = "Створити";
    t.remove = "Видалити";
    t.back = "Назад";
    t.empty = "Вільне місце";
    return t;
}
std::string none(const panel::Picture*) { return {}; }
}  // namespace

// 3.0.18-beta.106: the character select - its buttons a flex row by RmlUi, the cards where core charselect has them
// (the body moved as RmlUi placed it); its height kept frame after frame
TEST(uikit_charselect_buttons_live_cards_in_place) {
    std::vector<cs::Slot> slots(3);
    slots[0].used = true;
    slots[0].name = "Hero";
    cs::Show sh;
    cs::Layout L = cs::layout(1920, 1080, 1);
    static cl::Model m;
    cl::fill(m, L, slots, sh, texts(), none);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(cl::rml(), [&](Rml::DataModelConstructor& c) { cl::bind(c, m); }, 300, 100, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    const panel::Box coreSlot = L.slots[1], coreButton = L.buttons[2];
    int first = 0;
    for (int frame = 0; frame < 4; ++frame) {
        L = cs::layout(1920, 1080, 1);
        CHECK(cl::refit(d, L));
        if (frame == 0) first = L.h;
        cl::fill(m, L, slots, sh, texts(), none);
        rmlhost::update();
    }
    CHECK(std::abs(L.h - first) <= 1);
    CHECK(std::abs(L.slots[1].y - coreSlot.y) < 1.5f);
    CHECK(std::abs(L.buttons[2].y - coreButton.y) < 3);  // the buttons' row where core charselect has it
    cs::Hit h = cs::hitTest(L, mid(L.slots[1]).x, mid(L.slots[1]).y);
    CHECK(h.part == cs::Part::Slot);
    CHECK_EQ(h.index, 1);
    h = cs::hitTest(L, mid(L.buttons[3]).x, mid(L.buttons[3]).y);
    CHECK(h.part == cs::Part::Button);
    CHECK_EQ(h.index, 3);
    CHECK(cs::hitTest(L, mid(L.pageRight).x, mid(L.pageRight).y).part == cs::Part::PageRight);
    rmlhost::close(d);
}
