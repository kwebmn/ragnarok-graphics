#include "core/trade_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

#include "core/ui_style.hpp"

namespace uagfx::tradewin {
namespace {

using itemwin::Align;
using itemwin::Words;

// Its sizes at scale 1: 520 wide; the title 46, a side's head 26, cells 40 five a row 8 apart (two rows), the zeny row
// 34, the warning 22, the buttons 34, the hint 24; the two sides 16 apart
constexpr float kW = 520, kTitleH = 46, kHeadH = 26, kCell = 40, kGap = 8, kZenyH = 34, kWarnH = 22, kButtonH = 34;
constexpr float kHintH = 24, kSide = 18, kSidesGap = 16;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kGold2 = 0xC9A55B;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kGreen = 0x5FD08A, kRed = 0xFF7D6E;

std::string tipFirstOf(const Thing& x) {
    std::string s = x.refine > 0 ? "+" + std::to_string(x.refine) + " " + x.name : x.name;
    if (x.count > 1) s += " \xC3\x97" + itemwin::grouped(x.count);
    return s;
}

}  // namespace

std::string tipFirst(const Thing& x) { return tipFirstOf(x); }

bool Late::any() const { return zeny || std::any_of(std::begin(slots), std::end(slots), [](bool b) { return b; }); }

Late watchLate(const Side& before, const Side& now, bool meReady, Late was) {
    if (!meReady) return was;
    for (int i = 0; i < kSlots; ++i)
        if (now.slots[i].id != before.slots[i].id || now.slots[i].count != before.slots[i].count) was.slots[i] = true;
    if (now.zeny != before.zeny) was.zeny = true;
    return was;
}

bool canReady(const Side& me) { return !me.ready; }

bool canTrade(const Side& me, const Side& them, bool traded) { return me.ready && them.ready && !traded; }

Layout layout(int x, int y, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s, colW = (w - 2 * side - kSidesGap * s) / 2;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    const float headY = kTitleH * s;
    L.mineHead = panel::Box{side, headY, colW, kHeadH * s};
    L.theirHead = panel::Box{side + colW + kSidesGap * s, headY, colW, kHeadH * s};
    const float cell = kCell * s, gap = kGap * s;
    const float gridW = float(kColumns) * cell + float(kColumns - 1) * gap;
    const float gridY = headY + kHeadH * s + 6 * s;
    for (int sideNo = 0; sideNo < 2; ++sideNo) {
        const panel::Box& head = sideNo == 0 ? L.mineHead : L.theirHead;
        const float gx = head.x + (head.w - gridW) / 2;
        for (int i = 0; i < kSlots; ++i)
            L.cells[sideNo * kSlots + i] = panel::Box{gx + float(i % kColumns) * (cell + gap),
                                                      gridY + float(i / kColumns) * (cell + gap), cell, cell};
    }
    const float zenyY = gridY + 2 * cell + gap + 10 * s;
    L.mineZeny = panel::Box{L.mineHead.x, zenyY, colW, kZenyH * s};
    L.theirZeny = panel::Box{L.theirHead.x, zenyY, colW, kZenyH * s};
    L.zenyField = panel::Box{L.mineZeny.x + 56 * s, zenyY + 3 * s, colW - 56 * s, kZenyH * s - 6 * s};
    L.warn = panel::Box{side, zenyY + kZenyH * s + 4 * s, w - 2 * side, kWarnH * s};
    const float buttonsY = L.warn.y + L.warn.h + 4 * s;
    const float bw = (w - 2 * side - 2 * 10 * s) / 3;
    for (int i = 0; i < 3; ++i)
        L.buttons[i] = panel::Box{side + float(i) * (bw + 10 * s), buttonsY, bw, kButtonH * s};
    L.hint = panel::Box{side, buttonsY + kButtonH * s + 4 * s, w - 2 * side, kHintH * s};
    L.glass = panel::Box{0, 0, w, L.hint.y + L.hint.h + 6 * s};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (int i = 0; i < 2 * kSlots; ++i)
        if (L.cells[i].contains(x, y)) return Hit{Part::Cell, i};
    if (L.zenyField.contains(x, y)) return Hit{Part::Zeny, -1};
    for (int i = 0; i < 3; ++i)
        if (L.buttons[i].contains(x, y)) return Hit{Part::Button, i};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

}  // namespace uagfx::tradewin
