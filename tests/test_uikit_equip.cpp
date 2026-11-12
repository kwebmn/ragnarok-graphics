#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/equip_live.hpp"
#include "uikit/hits.hpp"

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
    CHECK(std::abs(float(L.w) - 640) < 2.5f);  // 3.0.18-beta.112: smaller (was 880)
    for (int i = 0; i < ew::kSlots; ++i) {
        const panel::Box p = mid(L.slots[i]);
        const ew::Hit h = ew::hitTest(L, p.x, p.y);
        CHECK(h.part == ew::Part::Slot);
        CHECK_EQ(h.index, i);
    }
    CHECK(L.slots[ew::HeadTop].x < L.centre.x);              // 3.0.18-beta.110: the original's left column
    CHECK(L.slots[ew::HeadMid].x >= L.centre.x + L.centre.w - 0.5f);  // and its right (rem rounding)
    CHECK(std::abs(L.slots[ew::HeadLow].y - L.slots[ew::HeadTop].y - 52) < 0.5f);
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

// 3.0.18-beta.113 (the owner: «чтобы имена не обрезались и не добавлялись точки»): a long name on two lines - its
// slot taller, the one under it lower, nothing cut
TEST(uikit_equip_long_names_wrap_and_grow_their_slot) {
    ew::Data d;
    d.slots[ew::Weapon].index = 7;
    d.slots[ew::Weapon].name = "Quadruple Critical Katana of Double Champion";
    d.slots[ew::Weapon].refine = 9;
    ew::Show sh;
    static el::Model m;
    el::fill(m, d, sh, texts(), 1, none, glyph);
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(el::rml(), [&](Rml::DataModelConstructor& c) { el::bind(c, m); }, 0, 0, &error);
    CHECK_EQ(error, std::string());
    ew::Layout L = ew::layout(0, 0, 1, d, sh, texts(), [](const std::string& s, float px, bool) {
        return float(s.size()) * px * 0.55f;
    });
    CHECK(el::refit(doc, L));
    CHECK(L.slots[ew::Weapon].h > 50);  // two lines
    CHECK(L.slots[ew::Garment].y >= L.slots[ew::Weapon].y + L.slots[ew::Weapon].h);
    CHECK(std::abs(L.slots[ew::HeadLow].h - 46) < 1.5f);  // a short one as before
    rmlhost::close(doc);
}

// 3.0.18-beta.114 (the owner: «нету кнопки для анманта пеко итд»), .116 (the owner chose «2 Значок між стрілками»): a
// sign between ◀ and ▶, found by its box (the window is passive); its words in a tip over it while the cursor is on
// it; none when there is nothing to take off
TEST(uikit_equip_mount_button_between_the_arrows) {
    ew::Data d;
    ew::Show sh;
    static el::Model m;
    el::fill(m, d, sh, texts(), 1, none, glyph);
    m.turns = true;
    m.mountOff = "Злізти з Пеко";
    m.mountIcon = "";
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(el::rml(), [&](Rml::DataModelConstructor& c) { el::bind(c, m); }, 0, 0, &error);
    CHECK_EQ(error, std::string());
    ew::Layout L = ew::layout(0, 0, 1, d, sh, texts(), [](const std::string& s, float px, bool) {
        return float(s.size()) * px * 0.55f;
    });
    CHECK(el::refit(doc, L));
    const std::vector<uikit::Placed> at = uikit::placesOf(doc, "mountoff");
    const std::vector<uikit::Placed> left = uikit::placesOf(doc, "turnleft"), right = uikit::placesOf(doc, "turnright");
    CHECK_EQ(at.size(), size_t(1));
    CHECK(left.size() == 1 && right.size() == 1);
    if (at.size() == 1 && left.size() == 1 && right.size() == 1) {
        const panel::Box& b = at[0].box;
        CHECK(b.x >= left[0].box.x + left[0].box.w && b.x + b.w <= right[0].box.x);  // between them
        CHECK(std::abs((b.y + b.h / 2) - (left[0].box.y + left[0].box.h / 2)) < 1.5f);  // in their row
        CHECK(std::abs(b.w - b.h) < 0.5f);  // round: a sign alone
    }
    CHECK(uikit::placesOf(doc, "mounttip").empty());  // the words only under the cursor
    m.mountHot = true;
    rmlhost::update();
    const std::vector<uikit::Placed> tip = uikit::placesOf(doc, "mounttip");
    CHECK_EQ(tip.size(), size_t(1));
    if (tip.size() == 1 && at.size() == 1) CHECK(tip[0].box.y + tip[0].box.h <= at[0].box.y + 0.5f);  // over it
    m.mountOff = "";
    m.mountHot = false;
    rmlhost::update();
    CHECK(uikit::placesOf(doc, "mountoff").empty());
    rmlhost::close(doc);
}

// 3.0.18-beta.117 (the owner: «при анмаунте надо сделать подтверждение да нет»): the sign pressed asks first - its
// words with «?» and Yes / No over it, both found by their boxes; no tip while asking
TEST(uikit_equip_mount_asks_yes_or_no) {
    ew::Data d;
    ew::Show sh;
    static el::Model m;
    el::fill(m, d, sh, texts(), 1, none, glyph);
    m.turns = true;
    m.mountOff = "Злізти з Пеко";
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(el::rml(), [&](Rml::DataModelConstructor& c) { el::bind(c, m); }, 0, 0, &error);
    CHECK_EQ(error, std::string());
    ew::Layout L = ew::layout(0, 0, 1, d, sh, texts(), [](const std::string& s, float px, bool) {
        return float(s.size()) * px * 0.55f;
    });
    CHECK(el::refit(doc, L));
    CHECK(uikit::placesOf(doc, "mountyes").empty());
    m.mountAsk = true;
    m.mountHot = true;
    m.askYes = "Так";
    m.askNo = "Ні";
    rmlhost::update();
    const auto yes = uikit::placesOf(doc, "mountyes"), no = uikit::placesOf(doc, "mountno");
    const auto sign = uikit::placesOf(doc, "mountoff");
    CHECK(yes.size() == 1 && no.size() == 1 && sign.size() == 1);
    CHECK(uikit::placesOf(doc, "mounttip").empty());
    if (yes.size() == 1 && no.size() == 1 && sign.size() == 1) {
        CHECK(yes[0].box.w > 10 && no[0].box.w > 10);
        CHECK(yes[0].box.x + yes[0].box.w <= no[0].box.x + 0.5f);       // Yes, then No
        CHECK(yes[0].box.y + yes[0].box.h <= sign[0].box.y + 0.5f);      // over the sign
        CHECK(yes[0].box.x >= L.centre.x - 0.5f && no[0].box.x + no[0].box.w <= L.centre.x + L.centre.w + 0.5f);
    }
    rmlhost::close(doc);
}

// 3.0.18-beta.217 (R-RASZHM's second picture: the sets «1 2 3» drawn over their hint, the row at the window's very left
// edge): the hint after the last set, the row in from the edge as the ammo's
TEST(uikit_equip_sets_row_beside_its_hint) {
    ew::Data d;
    ew::Show sh;
    static el::Model m;
    el::fill(m, d, sh, texts(), 1, none, glyph);
    using Set = el::Model::SetV;
    m.sets = {Set{"1", false, false}, Set{"2", true, false}, Set{"3", false, false}};
    m.setsHint = "Комплекти: клік — вдягти, права кнопка — запам'ятати вдягнене";
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(el::rml(), [&](Rml::DataModelConstructor& c) { el::bind(c, m); }, 100, 80, &error);
    CHECK_EQ(error, std::string());
    rmlhost::update();
    const std::vector<uikit::Placed> sets = uikit::placesOf(doc, "set");
    float ammoX = -1;
    for (const uikit::Placed& p : uikit::placesOf(doc, "slot"))
        if (p.index == ew::Ammo) ammoX = p.box.x;
    Rml::Element* hint = doc->QuerySelector(".setsnote");
    CHECK_EQ(sets.size(), size_t(3));
    CHECK(hint != nullptr && ammoX >= 0);
    if (sets.size() == 3 && hint && ammoX >= 0) {
        const float lastRight = sets[2].box.x + sets[2].box.w, hintX = hint->GetAbsoluteLeft() - doc->GetAbsoluteLeft();
        CHECK(hintX >= lastRight);            // after the sets, not over them
        CHECK(sets[0].box.x >= ammoX - 0.5f);  // in from the edge as the ammo's
    }
    rmlhost::close(doc);
}
