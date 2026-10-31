// 3.0.11-beta.97 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's
// «Снаряжение»): «Спорядження» redrawn (core/equip_window) - the slots round the character (heads, armour, garment,
// accessories on the left; the top head, weapon, shield, shoes, accessory on the right; the ammo under it), a worn
// thing's card dots, the controller's list of what fits a slot
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
    CHECK(L.slots[equipwin::HeadMid].x < L.centre.x && L.slots[equipwin::HeadTop].x > L.centre.x);  // two columns
    CHECK(L.slots[equipwin::AccL].y > L.slots[equipwin::Garment].y);  // down the column in the concept's order
    CHECK(L.slots[equipwin::Ammo].y > L.portrait.y + L.portrait.h);   // the ammo under the character
    const auto at = [&](const panel::Box& b) { return equipwin::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.slots[equipwin::Weapon]).part == equipwin::Part::Slot && at(L.slots[equipwin::Weapon]).index == 6);
    CHECK(at(L.close).part == equipwin::Part::Close);
    const std::vector<equipwin::Words> w = equipwin::words(L, d, sh, texts(), measure);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const equipwin::Words& x) { return x.text == text; });
    };
    CHECK(has("Спорядження") && has("Knife") && has("+4 Cotton Shirt") && has("Зброя") && has("Sora") &&
          has("Lv 72 / 40") && has("Кружечки — слоти карток"));
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
