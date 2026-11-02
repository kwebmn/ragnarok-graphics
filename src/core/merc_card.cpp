#include "core/merc_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::merccard {
namespace {

// At scale 1: the card 320 wide, 16 in; its title 30; the name 24; a bar's row 16 over its 7-high bar; a fact 18
constexpr float kW = 320, kPad = 16, kTitleH = 30, kNameH = 24, kRowH = 16, kBarH = 7, kLineH = 18, kButtonH = 34;
constexpr float kGap = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kHp = 0x4CC777, kSp = 0x3D7EEB, kDanger = 0xE0675C;
const char* const kStatNames[8] = {"ATK", "MATK", "HIT", "CRIT", "DEF", "MDEF", "FLEE", "ASPD"};

std::string ofMost(int now, int most) { return std::to_string(now) + " / " + std::to_string(most); }

}  // namespace

Kind kindOf(int cls) {
    if (cls >= 6017 && cls <= 6026) return Kind::Archer;
    if (cls >= 6037 && cls <= 6046) return Kind::Spearman;
    return Kind::Swordsman;
}

int gameButton(Button b) {
    switch (b) {
        case Button::Skills: return 0x11b;
        case Button::Dismiss: return 0x11c;
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
    for (int i = 0; i < 4; ++i) {
        L.facts[i] = panel::Box{pad, y, inner, kLineH * s};
        y += kLineH * s;
    }
    y += 8 * s;
    const float cw = (inner - kGap * s) / 2;
    for (int i = 0; i < 8; ++i)
        L.stats[i] = panel::Box{pad + float(i % 2) * (cw + kGap * s), y + float(i / 2) * kLineH * s, cw, kLineH * s};
    y += 4 * kLineH * s + 10 * s;
    L.skills = panel::Box{pad, y, inner, kButtonH * s};
    y += kButtonH * s + 6 * s;
    L.dismiss = panel::Box{w / 2 - 50 * s, y, 100 * s, 18 * s};
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
    if (L.skills.contains(x, y)) return Hit{Part::Button, Button::Skills};
    if (L.dismiss.contains(x, y)) return Hit{Part::Button, Button::Dismiss};
    return Hit{};
}

void paint(panel::Picture& out, const Layout& L, const Info& i, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 6 * s, kWhite, sh.hover == int(Button::Close) ? 0.14f : 0.06f);
    const auto bar = [&](const panel::Box& b, int now, int most, uint32_t rgb) {
        panel::roundRect(out, b, b.h / 2, kWhite, 0.08f);
        if (most > 0 && now > 0)
            panel::roundRect(out, panel::Box{b.x, b.y, b.w * std::min(1.0f, float(now) / float(most)), b.h}, b.h / 2,
                             rgb, 0.9f);
    };
    bar(L.hp, i.hp, i.maxHp, kHp);
    bar(L.sp, i.sp, i.maxSp, kSp);
    const bool hot = sh.hover == int(Button::Skills) || sh.focus;
    panel::roundRect(out, L.skills, 8 * s, kGold, hot ? 0.34f : 0.22f, kGold, sh.focus ? 0.95f : 0.8f,
                     (sh.focus ? 1.5f : 1.0f) * s);
    if (sh.hover == int(Button::Dismiss)) panel::roundRect(out, L.dismiss, 6 * s, kDanger, 0.12f);
}

std::vector<Words> words(const Layout& L, const Info& i, const Show& sh, const Texts& t) {
    (void)sh;
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.kinds[int(i.kind)], kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    out.push_back(Words{L.name, i.name, kGold2, 15 * s, true, Align::Left});
    std::string level = t.level;
    if (const size_t p = level.find("{1}"); p != std::string::npos) level.replace(p, 3, std::to_string(i.level));
    out.push_back(Words{L.level, level, kMuted, 12.5f * s, false, Align::Right});
    out.push_back(Words{L.hpLabel, "HP", kMuted, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.hpLabel, ofMost(i.hp, i.maxHp), kText, 11.5f * s, false, Align::Right});
    out.push_back(Words{L.spLabel, "SP", kMuted, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.spLabel, ofMost(i.sp, i.maxSp), kText, 11.5f * s, false, Align::Right});
    const std::string facts[4] = {i.ends, std::to_string(i.loyalty), std::to_string(i.summons),
                                  std::to_string(i.kills)};
    for (int k = 0; k < 4; ++k) {
        out.push_back(Words{L.facts[k], t.facts[k], kMuted, 11.5f * s, false, Align::Left});
        out.push_back(Words{L.facts[k], facts[k], k == 0 ? kGold2 : kText, 12 * s, k == 0, Align::Right});
    }
    for (int k = 0; k < 8; ++k) {
        out.push_back(Words{L.stats[k], kStatNames[k], kMuted, 11.5f * s, false, Align::Left});
        out.push_back(Words{L.stats[k], std::to_string(k < 7 ? i.stats[k] : (2000 - i.aspd) / 10), kText, 12 * s,
                            true, Align::Right});
    }
    out.push_back(Words{L.skills, t.skills, kText, 12.5f * s, true, Align::Center});
    out.push_back(Words{L.dismiss, t.dismiss, kDanger, 11 * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::merccard
