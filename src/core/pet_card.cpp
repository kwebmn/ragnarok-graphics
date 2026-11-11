#include "core/pet_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::petcard {
namespace {

// At scale 1: the card 300 wide, 16 in; its title 30; the name 24; a bar's row 18 over its 8-high bar; buttons 34
constexpr float kW = 300, kPad = 16, kTitleH = 30, kNameH = 24, kRowH = 18, kBarH = 8, kButtonH = 34, kGap = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kFull = 0x4CC777, kLove = 0xF06B8B;

}  // namespace

int hungerState(int hunger) {
    return hunger <= 10 ? 0 : hunger <= 25 ? 1 : hunger <= 75 ? 2 : hunger <= 90 ? 3 : 4;
}

int intimacyState(int intimacy) {
    return intimacy <= 100 ? 0 : intimacy <= 250 ? 1 : intimacy <= 750 ? 2 : intimacy <= 900 ? 3 : 4;
}

int gameChoice(Command c) { return int(c); }

bool allowed(const Info& i, Command c) { return c != Command::Unequip || i.accessory; }

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s;
    L.title = panel::Box{pad, pad, w - 2 * pad - 28 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 24 * s, pad + 3 * s, 24 * s, 24 * s};
    float y = pad + kTitleH * s + 2 * s;
    L.name = panel::Box{pad, y, w * 0.6f - pad, kNameH * s};
    L.level = panel::Box{w * 0.6f, y, w * 0.4f - pad, kNameH * s};
    y += kNameH * s + 8 * s;
    const auto bar = [&](panel::Box& label, panel::Box& b, panel::Box& state) {
        label = panel::Box{pad, y, (w - 2 * pad) / 2, kRowH * s};
        state = panel::Box{w / 2, y, w / 2 - pad, kRowH * s};
        y += kRowH * s + 2 * s;
        b = panel::Box{pad, y, w - 2 * pad, kBarH * s};
        y += kBarH * s + 10 * s;
    };
    bar(L.hungerLabel, L.hungerBar, L.hungerState);
    bar(L.intimacyLabel, L.intimacyBar, L.intimacyState);
    y += 2 * s;
    const float bw = (w - 2 * pad - kGap * s) / 2;
    for (int i = 0; i < 4; ++i)
        L.buttons[i] = panel::Box{pad + float(i % 2) * (bw + kGap * s), y + float(i / 2) * (kButtonH * s + kGap * s),
                                  bw, kButtonH * s};
    y += 2 * kButtonH * s + kGap * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(int(float(targetW) * 0.68f), 0, std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.30f), 0, std::max(0, targetH - L.h));
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (int i = 0; i < 4; ++i)
        if (L.buttons[i].contains(x, y)) return Hit{Part::Button, i};
    return Hit{};
}

}  // namespace uagfx::petcard
