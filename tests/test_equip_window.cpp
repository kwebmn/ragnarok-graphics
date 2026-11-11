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

