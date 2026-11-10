#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/equip_live.hpp"

using namespace uagfx;
namespace ew = uagfx::equipwin;
namespace el = uagfx::uikit::equip;

namespace {

ew::Texts texts() {
    ew::Texts t;
    t.title = "Спорядження";
    t.hotkey = "Alt+Q";
    const char* labels[ew::kSlots] = {"Середня голова", "Нижня голова", "Броня",  "Плащ",  "Аксесуар", "Верхня голова",
                                      "Зброя",          "Щит",          "Взуття", "Аксесуар", "Набої"};
    for (int i = 0; i < ew::kSlots; ++i) t.labels[i] = labels[i];
    t.empty = "—";
    t.cards = "Кружечки — слоти карток";
    t.drag = "Перетягніть річ сюди";
    t.level = "Lv {1} / {2}";
    t.pickTitle = "Що вдягнути: {1}";
    t.pickNone = "Нічого не підходить";
    return t;
}

panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }

std::string none(const panel::Picture*) { return {}; }
std::string glyph(int, float h, float& w) {
    w = h;
    return {};
}

}  // namespace

// 3.0.18-beta.96: «Спорядження» laid out by RmlUi; the core hit test on its boxes finds every slot (the ammo's in the
// middle), the controller's list over the middle when picking
TEST(uikit_equip_refit_finds_every_slot_and_the_picks) {
    ew::Data d;
    d.who = "Hero";
    d.job = "Swordman";
    d.baseLevel = 50;
    d.jobLevel = 30;
    d.stats = {{"ATK", "28 + 17"}, {"MATK", "10"}, {"DEF", "5"}, {"MDEF", "3"}};
    d.slots[ew::Weapon].index = 7;
    d.slots[ew::Weapon].name = "Sword";
    d.slots[ew::Weapon].slots = 3;
    d.slots[ew::Weapon].cards[0] = 4001;
    ew::Show sh;
    static el::Model m;
    el::fill(m, d, sh, texts(), 1, none, glyph);
    CHECK_EQ(m.level, std::string("Lv 50 / 30"));
    CHECK_EQ(m.left[2].dots.size(), size_t(3));  // 3.0.18-beta.110: the weapon third in the left column
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(el::rml(), [&](Rml::DataModelConstructor& c) { el::bind(c, m); }, 100, 80, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    ew::Layout L = ew::layout(0, 0, 1, d, sh, texts(), [](const std::string& s, float px, bool) {
        return float(s.size()) * px * 0.55f;
    });
    CHECK(el::refit(doc, L));
    CHECK(std::abs(float(L.w) - 880) < 2.5f);
    for (int i = 0; i < ew::kSlots; ++i) {
        const panel::Box p = mid(L.slots[i]);
        const ew::Hit h = ew::hitTest(L, p.x, p.y);
        CHECK(h.part == ew::Part::Slot);
        CHECK_EQ(h.index, i);
    }
    CHECK(L.slots[ew::HeadTop].x < L.centre.x);              // 3.0.18-beta.110: the original's left column
    CHECK(L.slots[ew::HeadMid].x >= L.centre.x + L.centre.w);  // and its right
    CHECK(std::abs(L.slots[ew::HeadLow].y - L.slots[ew::HeadTop].y - 64) < 0.5f);
    CHECK(L.slots[ew::Ammo].y > L.slots[ew::AccR].y + L.slots[ew::AccR].h);  // the ammo in the strip under
    CHECK(L.icons[ew::HeadTop].x < L.slots[ew::HeadTop].x + L.slots[ew::HeadTop].w / 2);  // the outer edges
    CHECK(L.icons[ew::HeadMid].x > L.slots[ew::HeadMid].x + L.slots[ew::HeadMid].w / 2);
    panel::Box p = mid(L.close);
    CHECK(ew::hitTest(L, p.x, p.y).part == ew::Part::Close);
    const int height = L.h;
    sh.picking = true;  // the controller's list over the middle
    sh.pickSlot = ew::Shield;
    d.picks = {ew::Pick{3, 2101, 0, 0x20, "Guard", nullptr}, ew::Pick{4, 2102, 4, 0x20, "Buckler", nullptr}};
    el::fill(m, d, sh, texts(), 1, none, glyph);
    rmlhost::update();
    CHECK(el::refit(doc, L));
    CHECK_EQ(L.h, height);
    CHECK_EQ(L.picks.size(), size_t(2));
    if (L.picks.size() < 2) return;
    p = mid(L.picks[1]);
    const ew::Hit h = ew::hitTest(L, p.x, p.y);
    CHECK(h.part == ew::Part::Pick);
    CHECK_EQ(h.index, 1);
    CHECK_EQ(m.pickTitle, std::string("Що вдягнути: Щит"));
    rmlhost::close(doc);
}
