#include "core/room_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::roomcard {
namespace {

// At scale 1: the card 560 wide, 16 in; its head 30; the lines 17 a row (14 rows), the members' column 150 wide (rows
// 22); the input row 32 with its buttons
constexpr float kW = 560, kPad = 16, kHeadH = 30, kRowH = 17, kMemberW = 150, kMemberRowH = 22, kInputH = 32;
constexpr float kGap = 8, kToolH = 26;
constexpr int kRows = 14;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000, kGreen = 0x4CAF7A, kRed = 0xD9534F;

}  // namespace

uint32_t rgbOf(uint32_t bgr) { return (bgr & 0xFF) << 16 | (bgr & 0xFF00) | (bgr >> 16 & 0xFF); }

Layout layout(const Card& c, int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s;
    L.badge = panel::Box{pad, pad + 4 * s, 86 * s, 22 * s};
    L.close = panel::Box{w - pad - 26 * s, pad + 2 * s, 26 * s, 26 * s};
    L.count = panel::Box{L.close.x - 64 * s, pad, 58 * s, kHeadH * s};
    L.title = panel::Box{L.badge.x + L.badge.w + 8 * s, pad, L.count.x - (L.badge.x + L.badge.w + 8 * s), kHeadH * s};
    const float top = pad + kHeadH * s + 6 * s;
    const float bodyH = kRowH * kRows * s;
    const float memberX = w - pad - kMemberW * s;
    L.lines = panel::Box{pad, top, memberX - kGap * s - pad, bodyH};
    L.lineRows = kRows;
    L.membersHead = panel::Box{memberX, top, kMemberW * s, 18 * s};
    float my = top + 20 * s;
    for (size_t i = 0; i < c.members.size(); ++i) {
        const panel::Box row{memberX, my, kMemberW * s, kMemberRowH * s};
        if (row.y + row.h > top + bodyH - (c.chosen >= 0 ? 3 * (kToolH + 4) * s : 0)) break;
        L.memberRows.push_back(row);
        my += kMemberRowH * s;
    }
    if (c.chosen >= 0 && c.chosen < int(c.members.size())) {  // the chosen one's actions under the members
        float ty = top + bodyH - kToolH * s;
        L.friendAdd = panel::Box{memberX, ty, kMemberW * s, kToolH * s};
        if (c.mine && !c.members[size_t(c.chosen)].owner) {
            ty -= (kToolH + 4) * s;
            L.owner = panel::Box{memberX, ty, kMemberW * s, kToolH * s};
            ty -= (kToolH + 4) * s;
            L.kick = panel::Box{memberX, ty, kMemberW * s, kToolH * s};
        }
    }
    const float iy = top + bodyH + kGap * s;
    const float bw = 96 * s;
    L.leave = panel::Box{w - pad - bw, iy, bw, kInputH * s};
    float right = L.leave.x - kGap * s;
    if (c.mine) {
        L.setup = panel::Box{right - 116 * s, iy, 116 * s, kInputH * s};
        right = L.setup.x - kGap * s;
    }
    L.input = panel::Box{pad, iy, right - pad, kInputH * s};
    const float h = iy + kInputH * s + pad;
    L.glass = panel::Box{0, 0, w, h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    L.x = std::min(int(24 * s), std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.16f), 0, std::max(0, targetH - L.h));
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    if (L.kick.contains(x, y)) return Hit{Part::Kick, -1};
    if (L.owner.contains(x, y)) return Hit{Part::Owner, -1};
    if (L.friendAdd.contains(x, y)) return Hit{Part::Friend, -1};
    for (size_t i = 0; i < L.memberRows.size(); ++i)
        if (L.memberRows[i].contains(x, y)) return Hit{Part::Member, int(i)};
    if (L.input.contains(x, y)) return Hit{Part::Input, -1};
    if (L.setup.contains(x, y)) return Hit{Part::Setup, -1};
    if (L.leave.contains(x, y)) return Hit{Part::Leave, -1};
    return Hit{};
}

}  // namespace uagfx::roomcard
