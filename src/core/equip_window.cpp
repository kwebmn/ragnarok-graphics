#include "core/equip_window.hpp"

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "core/ui_style.hpp"

namespace uagfx::equipwin {
namespace {

// The concept's sizes at scale 1: 880 wide, the title 46, a slot 56 high every 64, the middle 220, the footer 40
// 3.0.18-beta.112 (the owner: «сам диалог можно меньше сделать»): 640 wide (was 880), slots 46 every 52
constexpr float kW = 640, kTitleH = 46, kSide = 16, kSlotH = 46, kSlotStep = 52, kColumnW = 228, kIcon = 36;
constexpr float kCentreW = 152, kPortrait = 96, kFooterH = 40, kPickH = 34, kDot = 7, kDotStep = 10, kGlyph = 17;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xF3C653, kGold2 = 0xC9A55B, kTile = 0x1B1D24;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kBlue = 0x8FB8F0;

bool left(int slot) { return std::find(std::begin(kLeftColumn), std::end(kLeftColumn), slot) != std::end(kLeftColumn); }

int rowOf(int slot) {  // its place down its column (the original's)
    for (int r = 0; r < 5; ++r)
        if (kLeftColumn[r] == slot || kRightColumn[r] == slot) return r;
    return 0;
}

struct PadItem {
    padglyph::Button button;
    const std::string* words;
};

std::vector<PadItem> padItems(const Data& d, const Show& sh, const Texts& t) {
    if (sh.picking) return {{padglyph::Button::A, &t.put}, {padglyph::Button::B, &t.back}};
    const bool worn = sh.focus >= 0 && sh.focus < kSlots && d.slots[sh.focus].index > 0;
    if (!worn) return {{padglyph::Button::A, &t.put}, {padglyph::Button::B, &t.close}};
    return {{padglyph::Button::A, &t.off}, {padglyph::Button::X, &t.change}, {padglyph::Button::Y, &t.card},
            {padglyph::Button::B, &t.close}};
}

}  // namespace

const uint32_t kBits[kSlots] = {0x200, 0x001, 0x010, 0x004, 0x008, 0x100, 0x002, 0x020, 0x040, 0x080, 0x8000};

int padStep(int slot, int dx, int dy) {
    if (slot == Ammo) return dy < 0 ? AccR : slot;
    const bool l = left(slot);
    const int row = rowOf(slot);
    if (dy > 0) return row < 4 ? (l ? kLeftColumn : kRightColumn)[row + 1] : Ammo;
    if (dy < 0) return row > 0 ? (l ? kLeftColumn : kRightColumn)[row - 1] : slot;
    if (dx > 0) return l ? kRightColumn[row] : slot;
    if (dx < 0) return l ? slot : kLeftColumn[row];
    return slot;
}

int bitIndex(uint32_t bit) {
    if (bit == 0x8000) return 15;
    for (int i = 0; i < 10; ++i)
        if (bit & (1u << i)) return i;
    return -1;
}

std::vector<int> slotsOf(uint32_t location) {
    static const int kOrder[kSlots] = {HeadTop, HeadMid, HeadLow, Weapon, Shield, Armor, Garment, Shoes, AccL, AccR,
                                       Ammo};
    std::vector<int> out;
    for (int s : kOrder)
        if (location & kBits[s]) out.push_back(s);
    return out;
}

bool fits(uint32_t location, int slot) { return slot >= 0 && slot < kSlots && (location & kBits[slot]) != 0; }

int iconSide(float scale) { return int(std::lround(32 * scale)); }

Layout layout(int x, int y, float scale, const Data& d, const Show& sh, const Texts& t, const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s, top = (kTitleH + 10) * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    const float columnH = 4 * kSlotStep * s + kSlotH * s;
    const float rightX = w - side - kColumnW * s;
    for (int i = 0; i < 10; ++i) {
        const bool l = left(i);
        const float sy = top + float(rowOf(i)) * kSlotStep * s;  // 3.0.18-beta.110: the original's rows
        L.slots[i] = panel::Box{l ? side : rightX, sy, kColumnW * s, kSlotH * s};
        const float ix = l ? side + kColumnW * s - 8 * s - kIcon * s : rightX + 8 * s;
        L.icons[i] = panel::Box{ix, sy + (kSlotH - kIcon) / 2 * s, kIcon * s, kIcon * s};
        const Worn& wi = d.slots[i];
        if (wi.index > 0 && wi.slots > 0 && !wi.spill) {
            const float dw = float(std::min(wi.slots, 4)) * kDotStep * s - (kDotStep - kDot) * s;
            const float dx = l ? L.icons[i].x - 10 * s - dw : L.icons[i].x + L.icons[i].w + 10 * s;
            L.dots[i] = panel::Box{dx, sy + 40 * s, dw, kDot * s};
        }
    }
    L.centre = panel::Box{(w - kCentreW * s) / 2, top, kCentreW * s, columnH};
    L.portrait = panel::Box{L.centre.x + (L.centre.w - kPortrait * s) / 2, top + 4 * s, kPortrait * s, kPortrait * s};
    // 3.0.18-beta.110: the ammo in the strip under both columns, as the original window's foot
    L.slots[Ammo] = panel::Box{side, top + columnH + 8 * s, kColumnW * s, 48 * s};
    L.icons[Ammo] = panel::Box{L.slots[Ammo].x + 4 * s, L.slots[Ammo].y + 2 * s, kIcon * s, kIcon * s};
    if (sh.picking) {
        L.pickList = L.centre;
        const int most = int((L.pickList.h - 32 * s) / (kPickH * s));
        for (size_t i = 0; i < d.picks.size() && int(i) < most; ++i)
            L.picks.push_back(panel::Box{L.pickList.x + 6 * s, L.pickList.y + 30 * s + float(i) * kPickH * s,
                                         L.pickList.w - 12 * s, (kPickH - 2) * s});
    }
    L.footer = panel::Box{0, L.slots[Ammo].y + L.slots[Ammo].h + 6 * s, w, kFooterH * s};
    if (sh.pad) {  // the controller's buttons in the footer, from its right
        const std::vector<PadItem> items = padItems(d, sh, t);
        float right = w - side;
        L.padGlyphs.resize(items.size());
        L.padWords.resize(items.size());
        for (size_t i = items.size(); i-- > 0;) {
            const float ww = m(*items[i].words, 12 * s, false) + 2;
            const float gw = padglyph::widthOf(items[i].button, kGlyph * s);
            L.padWords[i] = panel::Box{right - ww, L.footer.y, ww, L.footer.h};
            L.padGlyphs[i] = panel::Box{L.padWords[i].x - 6 * s - gw, L.footer.y + (L.footer.h - kGlyph * s) / 2, gw,
                                        kGlyph * s};
            right = L.padGlyphs[i].x - 16 * s;
        }
    }
    L.glass = panel::Box{0, 0, w, L.footer.y + L.footer.h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (size_t i = 0; i < L.picks.size(); ++i)
        if (L.picks[i].contains(x, y)) return Hit{Part::Pick, int(i)};
    for (int i = 0; i < kSlots; ++i)
        if (L.slots[i].w > 0 && L.slots[i].contains(x, y)) return Hit{Part::Slot, i};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

}  // namespace uagfx::equipwin
