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

void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

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

void paint(panel::Picture& out, const Layout& L, const Data& d) {
    const float s = L.s;
    panel::roundRect(out, L.pill, 8 * s, kCard, 0.86f, kWhite, 0.10f, 1.0f * s);
    if (d.icon && d.icon->width > 0 && L.icon.w > 0) {
        panel::roundRect(out, L.icon, 4 * s, kBlack, 0.5f);
        put(out, *d.icon, L.icon);
    }
    panel::roundRect(out, L.bar, L.bar.h / 2, kBlack, 0.6f, kWhite, 0.08f, 1.0f * s);
    const float done = std::clamp(d.done, 0.0f, 1.0f);
    if (done > 0) {
        const panel::Box fill{L.bar.x, L.bar.y, std::max(L.bar.h, L.bar.w * done), L.bar.h};
        const panel::Box glow{fill.x - 1.5f * s, fill.y - 1.5f * s, fill.w + 3 * s, fill.h + 3 * s};
        panel::roundRect(out, glow, glow.h / 2, kGold2, 0.22f);
        panel::roundRect(out, fill, fill.h / 2, kGold, 1.0f);
        const panel::Box top{fill.x, fill.y, fill.w, fill.h * 0.5f};  // its upper half lighter: the concept's gradient
        panel::roundRect(out, top, top.h / 2, kGold2, 0.8f);
    }
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
