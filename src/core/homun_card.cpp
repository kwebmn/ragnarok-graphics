#include "core/homun_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::homuncard {
namespace {

// At scale 1: the card 320 wide, 16 in; its title 30; the name 24; a bar's row 16 over its 7-high bar; a stat 18
constexpr float kW = 320, kPad = 16, kTitleH = 30, kNameH = 24, kRowH = 16, kBarH = 7, kStatH = 18, kButtonH = 34;
constexpr float kGap = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kHp = 0x4CC777, kSp = 0x3D7EEB, kExp = 0xC9A55B, kFull = 0x8BC34A;
constexpr uint32_t kLove = 0xF06B8B, kDanger = 0xE0675C;
const char* const kStatNames[8] = {"ATK", "MATK", "HIT", "CRIT", "DEF", "MDEF", "FLEE", "ASPD"};

std::string ofMost(long long now, long long most) { return std::to_string(now) + " / " + std::to_string(most); }

}  // namespace

int intimacyState(int intimacy) {
    const int edges[7] = {3, 10, 100, 250, 750, 910, 1000};
    for (int i = 0; i < 7; ++i)
        if (intimacy <= edges[i]) return i;
    return 7;
}

int aspd(int delay) { return (2000 - delay) / 10; }

int gameButton(Button b) {
    switch (b) {
        case Button::Feed: return 0xe0;
        case Button::Skills: return 0xdf;
        case Button::Delete: return 0xe1;
        case Button::Close: break;
    }
    return 0x87;
}

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad;
    L.title = panel::Box{pad, pad, inner - 28 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 24 * s, pad + 3 * s, 24 * s, 24 * s};
    float y = pad + kTitleH * s + 2 * s;
    L.name = panel::Box{pad, y, inner * 0.6f, kNameH * s};
    L.level = panel::Box{pad + inner * 0.6f, y, inner * 0.4f, kNameH * s};
    y += kNameH * s + 6 * s;
    const auto bar = [&](panel::Box& label, panel::Box& b) {
        label = panel::Box{pad, y, inner, kRowH * s};
        y += kRowH * s + 1 * s;
        b = panel::Box{pad, y, inner, kBarH * s};
        y += kBarH * s + 7 * s;
    };
    bar(L.hpLabel, L.hp);
    bar(L.spLabel, L.sp);
    bar(L.expLabel, L.exp);
    bar(L.hungerLabel, L.hunger);
    L.intimacyLabel = panel::Box{pad, y, inner, kRowH * s};
    L.intimacy = panel::Box{pad, y, inner, kRowH * s};
    y += kRowH * s + 8 * s;
    const float cw = (inner - kGap * s) / 2;
    for (int i = 0; i < 8; ++i)
        L.stats[i] = panel::Box{pad + float(i % 2) * (cw + kGap * s), y + float(i / 2) * kStatH * s, cw, kStatH * s};
    y += 4 * kStatH * s + 10 * s;
    const float bw = (inner - kGap * s) / 2;
    L.buttons[0] = panel::Box{pad, y, bw, kButtonH * s};
    L.buttons[1] = panel::Box{pad + bw + kGap * s, y, bw, kButtonH * s};
    y += kButtonH * s + 6 * s;
    L.remove = panel::Box{w / 2 - 50 * s, y, 100 * s, 18 * s};
    y += 18 * s + pad / 2;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(int(float(targetW) * 0.68f), 0, std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.22f), 0, std::max(0, targetH - L.h));
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Button, Button::Close};
    if (L.buttons[0].contains(x, y)) return Hit{Part::Button, Button::Feed};
    if (L.buttons[1].contains(x, y)) return Hit{Part::Button, Button::Skills};
    if (L.remove.contains(x, y)) return Hit{Part::Button, Button::Delete};
    return Hit{};
}

void paint(panel::Picture& out, const Layout& L, const Info& i, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 6 * s, kWhite, sh.hover == int(Button::Close) ? 0.14f : 0.06f);
    const auto bar = [&](const panel::Box& b, long long now, long long most, uint32_t rgb) {
        panel::roundRect(out, b, b.h / 2, kWhite, 0.08f);
        if (most > 0 && now > 0)
            panel::roundRect(out, panel::Box{b.x, b.y, b.w * std::min(1.0f, float(now) / float(most)), b.h}, b.h / 2,
                             rgb, 0.9f);
    };
    bar(L.hp, i.hp, i.maxHp, kHp);
    bar(L.sp, i.sp, i.maxSp, kSp);
    bar(L.exp, i.exp, i.maxExp, kExp);
    bar(L.hunger, i.hunger, 100, kFull);
    for (int k = 0; k < 2; ++k) {
        const Button b = k == 0 ? Button::Feed : Button::Skills;
        const bool hot = sh.hover == int(b) || sh.focus == int(b);
        panel::roundRect(out, L.buttons[k], 8 * s, k == 0 ? kGold : kWhite,
                         k == 0 ? (hot ? 0.34f : 0.22f) : (hot ? 0.12f : 0.06f),
                         sh.focus == int(b) ? kGold : k == 0 ? kGold : kWhite, sh.focus == int(b) ? 0.95f : 0.5f,
                         (sh.focus == int(b) ? 1.5f : 1.0f) * s);
    }
    if (sh.hover == int(Button::Delete)) panel::roundRect(out, L.remove, 6 * s, kDanger, 0.12f);
}

std::vector<Words> words(const Layout& L, const Info& i, const Show& sh, const Texts& t) {
    (void)sh;
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    out.push_back(Words{L.name, i.name, kGold2, 15 * s, true, Align::Left});
    std::string level = t.level;
    if (const size_t p = level.find("{1}"); p != std::string::npos) level.replace(p, 3, std::to_string(i.level));
    out.push_back(Words{L.level, level, kMuted, 12.5f * s, false, Align::Right});
    out.push_back(Words{L.hpLabel, "HP", kMuted, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.hpLabel, ofMost(i.hp, i.maxHp), kText, 11.5f * s, false, Align::Right});
    out.push_back(Words{L.spLabel, "SP", kMuted, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.spLabel, ofMost(i.sp, i.maxSp), kText, 11.5f * s, false, Align::Right});
    out.push_back(Words{L.expLabel, "EXP", kMuted, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.expLabel, ofMost(i.exp, i.maxExp), kText, 11.5f * s, false, Align::Right});
    out.push_back(Words{L.hungerLabel, t.hunger, kMuted, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.hungerLabel, ofMost(i.hunger, 100), kText, 11.5f * s, false, Align::Right});
    out.push_back(Words{L.intimacyLabel, t.intimacy, kMuted, 11.5f * s, false, Align::Left});
    out.push_back(
        Words{L.intimacy, t.intimacyStates[intimacyState(i.intimacy)], kLove, 12 * s, true, Align::Right});
    for (int k = 0; k < 8; ++k) {
        out.push_back(Words{L.stats[k], kStatNames[k], kMuted, 11.5f * s, false, Align::Left});
        out.push_back(Words{L.stats[k], std::to_string(k < 7 ? i.stats[k] : aspd(i.aspd)), kText, 12 * s, true,
                            Align::Right});
    }
    out.push_back(Words{L.buttons[0], t.feed, kText, 12.5f * s, true, Align::Center});
    out.push_back(Words{L.buttons[1], t.skills, kText, 12.5f * s, false, Align::Center});
    out.push_back(Words{L.remove, t.remove, kDanger, 11 * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::homuncard
