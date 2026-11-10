// 3.0.11-beta.97 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's
// «Снаряжение»): «Спорядження» redrawn (core/equip_window) - the slots round the character (heads, armour, garment,
// accessories on the left; the top head, weapon, shield, shoes, accessory on the right; the ammo under it), a worn
// thing's card dots, the controller's list of what fits a slot
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

#include "core/equip_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

float measure(const std::string& t, float px, bool bold) { return float(t.size()) * px * (bold ? 0.6f : 0.55f); }

equipwin::Texts texts() {
    equipwin::Texts t;
    t.title = "Спорядження";
    t.hotkey = "Alt+Q";
    const char* labels[equipwin::kSlots] = {"Середина голови", "Низ голови", "Броня",    "Плащ",
                                            "Аксесуар",        "Верх голови", "Зброя",   "Щит",
                                            "Взуття",          "Аксесуар",    "Набої"};
    for (int i = 0; i < equipwin::kSlots; ++i) t.labels[i] = labels[i];
    t.empty = "\xE2\x80\x94";
    t.cards = "Кружечки — слоти карток";
    t.drag = "Перетягніть предмет з речей на слот";
    t.level = "Lv {1} / {2}";
    t.pickTitle = "Що вдягнути: {1}";
    t.pickNone = "Нічого не підходить";
    t.off = "Зняти";
    t.put = "Вдягнути";
    t.change = "Замінити";
    t.card = "Картка";
    t.close = "Закрити";
    t.back = "Назад";
    return t;
}

equipwin::Data wizard() {
    equipwin::Data d;
    d.who = "Sora";
    d.job = "Wizard";
    d.baseLevel = 72;
    d.jobLevel = 40;
    equipwin::Worn knife;
    knife.index = 3;
    knife.id = 1201;
    knife.name = "Knife";
    knife.location = 0x02;
    knife.slots = 4;
    knife.cards[0] = 4001;
    d.slots[equipwin::Weapon] = knife;
    equipwin::Worn shirt;
    shirt.index = 5;
    shirt.id = 2301;
    shirt.name = "Cotton Shirt";
    shirt.location = 0x10;
    shirt.refine = 4;
    d.slots[equipwin::Armor] = shirt;
    return d;
}

}  // namespace

TEST(equip_window_slots_by_where_a_thing_is_worn) {
    CHECK_EQ(equipwin::bitIndex(0x0001), 0);    // the low head: the game's take-off slot 0
    CHECK_EQ(equipwin::bitIndex(0x0200), 9);    // the middle head
    CHECK_EQ(equipwin::bitIndex(0x8000), 15);   // the ammo
    CHECK(equipwin::slotsOf(0x0022) == std::vector<int>({equipwin::Weapon, equipwin::Shield}));  // two hands
    CHECK(equipwin::slotsOf(0x0301) ==
          std::vector<int>({equipwin::HeadTop, equipwin::HeadMid, equipwin::HeadLow}));  // a whole head's
    CHECK(equipwin::slotsOf(0x0088) == std::vector<int>({equipwin::AccL, equipwin::AccR}));  // either hand's ring
    CHECK(equipwin::fits(0x0088, equipwin::AccR) && equipwin::fits(0x0022, equipwin::Shield));
    CHECK(!equipwin::fits(0x0010, equipwin::Shoes) && equipwin::fits(0x8000, equipwin::Ammo));
}

TEST(equip_window_layout_round_the_character) {
    const equipwin::Data d = wizard();
    equipwin::Show sh;
    const equipwin::Layout L = equipwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    // 3.0.18-beta.110 (the owner: «максимально похожем на оригинальный вариант»): the game's own window's places -
    // left the top and low heads, the right hand, the robe, the right accessory; right the middle head, the body, the
    // left hand, the shoes, the left accessory; the ammo in the strip under both columns
    CHECK(L.slots[equipwin::HeadTop].x < L.centre.x && L.slots[equipwin::HeadMid].x > L.centre.x);
    CHECK(L.slots[equipwin::Weapon].x < L.centre.x && L.slots[equipwin::Shield].x > L.centre.x);
    CHECK(L.slots[equipwin::AccR].x < L.centre.x && L.slots[equipwin::AccL].x > L.centre.x);
    CHECK(L.slots[equipwin::HeadLow].y > L.slots[equipwin::HeadTop].y);
    CHECK(L.slots[equipwin::Weapon].y > L.slots[equipwin::HeadLow].y);
    CHECK(L.slots[equipwin::Garment].y > L.slots[equipwin::Weapon].y);
    CHECK(L.slots[equipwin::AccR].y > L.slots[equipwin::Garment].y);
    CHECK(std::abs(L.slots[equipwin::Armor].y - L.slots[equipwin::HeadLow].y) < 0.5f);  // body beside the low head
    CHECK(L.slots[equipwin::Ammo].y > L.slots[equipwin::AccR].y + L.slots[equipwin::AccR].h);
    const auto at = [&](const panel::Box& b) { return equipwin::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.slots[equipwin::Weapon]).part == equipwin::Part::Slot && at(L.slots[equipwin::Weapon]).index == 6);
    CHECK(at(L.close).part == equipwin::Part::Close);
    const std::vector<equipwin::Words> w = equipwin::words(L, d, sh, texts(), measure);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const equipwin::Words& x) { return x.text == text; });
    };
    CHECK(has("Спорядження") && has("Knife") && has("+4 Cotton Shirt") && has("Зброя") &&
          has("Кружечки — слоти карток"));
    CHECK(!has("Lv 72 / 40"));  // 3.0.18-beta.110: the character's numbers left to «Характеристики»
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    equipwin::paint(p, L, d, sh);
    CHECK(L.dots[equipwin::Weapon].w > 0 && L.dots[equipwin::Armor].w == 0);  // a slotted thing's dots alone
}

TEST(equip_window_the_controllers_pick_list) {
    equipwin::Data d = wizard();
    equipwin::Pick a;
    a.index = 9;
    a.name = "Sandals";
    a.location = 0x40;
    d.picks = {a};
    equipwin::Show sh;
    sh.pad = true;
    sh.focus = equipwin::Shoes;
    sh.picking = true;
    sh.pickSlot = equipwin::Shoes;
    const equipwin::Layout L = equipwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    CHECK(L.picks.size() == 1 && L.pickList.w > 0);
    const std::vector<equipwin::Words> w = equipwin::words(L, d, sh, texts(), measure);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const equipwin::Words& x) { return x.text == text; });
    };
    CHECK(has("Що вдягнути: Взуття") && has("Sandals") && has("Вдягнути") && has("Назад"));
    CHECK(equipwin::hitTest(L, L.picks[0].x + 5, L.picks[0].y + 5).part == equipwin::Part::Pick);
}

// 3.0.11-beta.110 (the concept «Концепция окон и инвентаря», stage 2): a thing of the bag dragged - the slots it fits
// lit in gold, the rest as they are
// 3.0.18-beta.110: the controller's steps in the original's columns - down the left one, across to its row's twin
TEST(equip_window_pad_steps_in_the_originals_columns) {
    using namespace equipwin;
    CHECK_EQ(padStep(HeadTop, 0, 1), int(HeadLow));
    CHECK_EQ(padStep(HeadLow, 0, 1), int(Weapon));
    CHECK_EQ(padStep(Weapon, 1, 0), int(Shield));
    CHECK_EQ(padStep(Shield, -1, 0), int(Weapon));
    CHECK_EQ(padStep(AccR, 0, 1), int(Ammo));
    CHECK_EQ(padStep(AccL, 0, 1), int(Ammo));
    CHECK_EQ(padStep(Ammo, 0, -1), int(AccR));
    CHECK_EQ(padStep(HeadTop, 0, -1), int(HeadTop));
    CHECK_EQ(padStep(HeadTop, -1, 0), int(HeadTop));
    CHECK_EQ(padStep(HeadMid, 1, 0), int(HeadMid));
}

TEST(equip_window_lights_the_slots_a_dragged_thing_fits) {
    const equipwin::Data d = wizard();
    equipwin::Show sh;
    const equipwin::Layout L = equipwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    const auto painted = [&](const equipwin::Show& show) {
        panel::Picture p;
        p.width = L.w;
        p.height = L.h;
        p.px.assign(size_t(L.w) * size_t(L.h), 0u);
        equipwin::paint(p, L, d, show);
        return p;
    };
    const panel::Picture plain = painted(sh);
    sh.dropFits = 1u << equipwin::Weapon;  // a bow dragged
    const panel::Picture lit = painted(sh);
    const auto edge = [](const panel::Picture& p, const panel::Box& b) {
        return p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x) + 1)];
    };
    CHECK(((edge(lit, L.slots[equipwin::Weapon]) >> 16) & 0xFF) >
          ((edge(plain, L.slots[equipwin::Weapon]) >> 16) & 0xFF) + 20);  // gold: more red
    CHECK(edge(lit, L.slots[equipwin::Armor]) == edge(plain, L.slots[equipwin::Armor]));
}


// 3.0.18-beta.29 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5): uaUI's «Спорядження» - the card
// dots (gold where a card sits), the controller's glyphs by the footer's words
TEST(equip_window_view_places_the_dots_and_glyphs) {
    const equipwin::Data d = wizard();
    equipwin::Show sh;
    const equipwin::Layout L = equipwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    const equipwin::View v = equipwin::view(L, d, sh);
    int weapon = 0, gold = 0;
    for (const equipwin::Dot& x : v.dots) {
        weapon += x.slot == equipwin::Weapon;
        gold += x.card;
        CHECK(x.cx >= L.dots[x.slot].x && x.cx <= L.dots[x.slot].x + L.dots[x.slot].w && x.r > 0);
    }
    CHECK(weapon == 4 && gold == 1 && v.glyphs.empty());
    sh.pad = true;
    sh.focus = equipwin::Weapon;
    const equipwin::Layout P = equipwin::layout(0, 0, 1.0f, d, sh, texts(), measure);
    const equipwin::View pv = equipwin::view(P, d, sh);
    CHECK(!pv.glyphs.empty() && pv.glyphs.size() == P.padGlyphs.size() && pv.glyphs[0].box.w > 0);
}
