#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::equipwin {

// 3.0.11-beta.97 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's
// «Снаряжение»): «Спорядження» (Alt+Q) redrawn in style A - the slots round the character as the concept's mockup has
// them (the middle and low heads, armour, garment, an accessory on the left; the top head, weapon, shield, shoes, an
// accessory on the right; the ammo under the character), a worn thing's icon, its refine and name, dots for its card
// slots (gold where a card sits); the character's portrait, name, job, levels and battle numbers in the middle; under
// it what the mouse and the controller do. The controller's A on an empty slot (X on a full one) lists what in the bag
// fits it. The parts that need no Windows; the words by GDI (win/equip_window).
constexpr int kSlots = 11;
enum Slot : int { HeadMid, HeadLow, Armor, Garment, AccL, HeadTop, Weapon, Shield, Shoes, AccR, Ammo };
extern const uint32_t kBits[kSlots];  // each slot's worn-location bit (the game's: 0x200 the middle head ...)

int bitIndex(uint32_t bit);  // the slot number the game's take-off asks with (FUN_00628bd0): 0..9, 15 the ammo
std::vector<int> slotsOf(uint32_t location);  // the slots a thing worn there fills, the one showing it first
bool fits(uint32_t location, int slot);       // a thing of the bag that goes there

struct Worn {
    int index = 0, id = 0, refine = 0, slots = 0, count = 0;
    uint32_t location = 0;
    int cards[4] = {};
    bool identified = true;
    bool spill = false;  // shown in another slot already (a two-handed weapon's shield): dimmed here
    std::string name;
    const panel::Picture* icon = nullptr;
};
struct Pick {  // a thing of the bag that fits the slot being filled
    int index = 0, id = 0, refine = 0;
    uint32_t location = 0;
    std::string name;
    const panel::Picture* icon = nullptr;
};
struct Data {
    Worn slots[kSlots];
    std::string who, job;
    int baseLevel = 0, jobLevel = 0;
    const panel::Picture* portrait = nullptr;
    std::vector<std::pair<std::string, std::string>> stats;  // «ATK», «28 + 17» ...
    std::vector<Pick> picks;
};

struct Texts {
    std::string title, hotkey;
    std::string labels[kSlots];
    std::string empty, cards, drag, level;  // «—», «Кружечки — слоти карток», ..., «Lv {1} / {2}»
    std::string pickTitle, pickNone;        // «Що вдягнути: {1}», «Нічого не підходить»
    std::string off, put, change, card, close, back;  // the controller's
};

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;

struct Show {
    int hoverSlot = -1, hoverPick = -1;
    int focus = -1;  // the controller's slot
    bool hoverClose = false, pad = false, sony = false;
    bool picking = false;  // the controller's list of what fits `pickSlot`
    int pickSlot = -1, pickFocus = 0;
    uint32_t dropFits = 0;  // 3.0.11-beta.110: bit i - slot i takes the thing dragged (lit in gold)
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, centre, portrait, footer, pickList;
    panel::Box slots[kSlots], icons[kSlots], dots[kSlots];
    std::vector<panel::Box> picks;
    std::vector<panel::Box> padGlyphs, padWords;
};
Layout layout(int x, int y, float scale, const Data& d, const Show& sh, const Texts& t, const Measure& m);
int iconSide(float scale);

enum class Part : uint8_t { None, Close, Slot, Pick, Title, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);

void paint(panel::Picture& out, const Layout& L, const Data& d, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Data& d, const Show& sh, const Texts& t, const Measure& m);

}  // namespace uagfx::equipwin
