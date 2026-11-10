#include "core/cast_bar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace uagfx::castbar {
namespace {

// The concept's sizes at scale 1 (build13/15 .castw): the icon 22, the pill 3 over and under it, 4 before and 9 after,
// the words 14 bold 6 apart; the bar 150 x 7, 4 under the pill; 3 round all for the bar's glow
constexpr float kIcon = 22, kPadY = 3, kPadL = 4, kPadR = 9, kGap = 6, kFont = 14, kBarW = 150, kBarH = 7;
constexpr float kBarGap = 4, kRoom = 3;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xC9A55B, kGold2 = 0xE6CB8A;
constexpr uint32_t kText = 0xECEFF5, kSoft = 0xD5D9E1;

}  // namespace

int iconSide(float scale) { return int(std::lround(kIcon * (scale > 0 ? scale : 1.0f))); }

Layout layout(const Data& d, int centreX, int barTop, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float nameW = d.name.empty() ? 0 : panel::wordsWidth(d.name, kFont * s, true) + 2 * s;
    const float leftW = panel::wordsWidth(d.left, kFont * s, true) + 2 * s;
    const bool icon = d.icon && d.icon->width > 0;
    float pillW = kPadL * s + (icon ? kIcon * s + kGap * s : 4 * s) + (nameW > 0 ? nameW + kGap * s : 0) + leftW +
                  kPadR * s;
    const float pillH = kIcon * s + 2 * kPadY * s, room = kRoom * s;
    L.w = int(std::ceil(std::max(pillW, kBarW * s) + 2 * room));
    L.h = int(std::ceil(pillH + kBarGap * s + kBarH * s + 2 * room));
    const float mid = float(L.w) / 2;
    L.pill = panel::Box{mid - pillW / 2, room, pillW, pillH};
    float x = L.pill.x + kPadL * s;
    if (icon) {
        L.icon = panel::Box{x, room + kPadY * s, kIcon * s, kIcon * s};
        x += kIcon * s + kGap * s;
    } else {
        x += 4 * s;
    }
    if (nameW > 0) {
        L.name = panel::Box{x, L.pill.y, nameW, pillH};
        x += nameW + kGap * s;
    }
    L.left = panel::Box{x, L.pill.y, leftW, pillH};
    L.bar = panel::Box{mid - kBarW * s / 2, L.pill.y + pillH + kBarGap * s, kBarW * s, kBarH * s};
    L.x = centreX - L.w / 2;
    L.y = barTop - int(std::lround(L.bar.y));
    return L;
}

std::vector<Words> words(const Layout& L, const Data& d) {
    std::vector<Words> out;
    const float s = L.s;
    if (!d.name.empty() && L.name.w > 0) out.push_back(Words{L.name, d.name, kText, kFont * s, true, Align::Left});
    out.push_back(Words{L.left, d.left, kSoft, kFont * s, true, Align::Left});
    return out;
}

std::string seconds(int ms, char decimal, const std::string& unit) {
    const int tenths = std::max(0, (ms + 99) / 100);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d%c%d", tenths / 10, decimal, tenths % 10);
    return std::string(buf) + "\xC2\xA0" + unit;
}

}  // namespace uagfx::castbar
