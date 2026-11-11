#include "core/merc_block.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/settings/strings.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::mercblock {
namespace {

constexpr float kWidth = 220, kHeight = 84, kPad = 10;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kHp = 0x5BC07A, kSp = 0x5B8CE0, kWarm = 0xE8A25B;
constexpr int kEndingSeconds = 5 * 60;

}  // namespace

std::string leftText(int seconds, const std::string& minutes, const std::string& hours) {
    if (seconds <= 0) return {};
    const int all = (seconds + 59) / 60;
    if (all < 60) return settings::fmt(minutes, {std::to_string(all)});
    char mm[8];
    std::snprintf(mm, sizeof mm, "%02d", all % 60);
    return settings::fmt(hours, {std::to_string(all / 60), mm});
}

bool ending(int seconds) { return seconds > 0 && seconds < kEndingSeconds; }

Layout layout(int left, int top, int width, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = width > 0 ? float(width) : kWidth * s;
    L.glass = panel::Box{0, 0, w, kHeight * s};
    const float in = w - 2 * kPad * s;
    L.icon = panel::Box{kPad * s, 8 * s, 24 * s, 24 * s};
    L.kind = panel::Box{(kPad + 32) * s, 6 * s, in - 32 * s - 50 * s, 16 * s};
    L.level = panel::Box{w - kPad * s - 50 * s, 6 * s, 50 * s, 16 * s};
    L.name = panel::Box{(kPad + 32) * s, 22 * s, in - 32 * s - 80 * s, 14 * s};
    L.hpText = panel::Box{w - kPad * s - 80 * s, 22 * s, 80 * s, 14 * s};
    L.hp = panel::Box{kPad * s, 42 * s, in, 7 * s};
    L.sp = panel::Box{kPad * s, 53 * s, in, 5 * s};
    L.left = panel::Box{kPad * s, 63 * s, in * 0.55f, 15 * s};
    L.loyalty = panel::Box{kPad * s + in * 0.55f, 63 * s, in * 0.45f, 15 * s};
    L.x = left;
    L.y = top;
    L.w = int(std::ceil(L.glass.w));
    L.h = int(std::ceil(L.glass.h));
    return L;
}

bool hit(const Layout& L, float x, float y) { return L.glass.contains(x, y); }

std::vector<Words> words(const Layout& L, const Info& i, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.kind, t.kinds[int(i.kind)], kText, 12 * s, true, Align::Left});
    out.push_back(Words{L.level, settings::fmt(t.level, {std::to_string(i.level)}), kMuted, 11 * s, false,
                        Align::Right});
    out.push_back(Words{L.name, i.name, kMuted, 11 * s, false, Align::Left});
    out.push_back(Words{L.hpText, std::to_string(i.hp) + " / " + std::to_string(i.maxHp), kText, 10.5f * s, false,
                        Align::Right});
    if (!t.left.empty())
        out.push_back(Words{L.left, t.left, ending(i.secondsLeft) ? kWarm : kMuted, 11 * s, ending(i.secondsLeft),
                            Align::Left});
    out.push_back(Words{L.loyalty, t.loyalty + ' ' + std::to_string(i.loyalty), kMuted, 11 * s, false, Align::Right});
    return out;
}

}  // namespace uagfx::mercblock
