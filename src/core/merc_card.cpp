#include "core/merc_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/companion_skills.hpp"
#include "core/ui_style.hpp"

namespace uagfx::merccard {
namespace {

// At scale 1: the card 320 wide, 16 in; its title 30; the name 24; a bar's row 16 over its 7-high bar; a fact 18
constexpr float kW = 320, kPad = 16, kTitleH = 30, kNameH = 24, kRowH = 16, kBarH = 7, kLineH = 18;
constexpr float kGap = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kHp = 0x4CC777, kSp = 0x3D7EEB, kDanger = 0xE0675C;
const char* const kStatNames[8] = {"ATK", "MATK", "HIT", "CRIT", "DEF", "MDEF", "FLEE", "ASPD"};

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

Layout layout(int targetW, int targetH, float scale, int skillCount) {
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
    L.skills = panel::Box{pad, y, inner, kLineH * s + 4 * s};  // 3.0.11-beta.218: the list's heading
    y += kLineH * s + 6 * s;
    for (int k = 0; k < std::max(1, skillCount); ++k) {
        L.skillRows.push_back(panel::Box{pad, y, inner, kLineH * s});
        const companion::RowParts rp = companion::rowParts(L.skillRows.back(), s, false);  // 3.0.16-beta.31
        L.skillLess.push_back(rp.less);
        L.skillMore.push_back(rp.more);
        L.skillLevel.push_back(rp.level);
        y += kLineH * s;
    }
    y += 10 * s;
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
    if (L.dismiss.contains(x, y)) return Hit{Part::Button, Button::Dismiss};
    for (size_t k = 0; k < L.skillRows.size(); ++k) {  // 3.0.16-beta.31
        if (L.skillLess[k].contains(x, y)) return Hit{Part::Less, Button::Close, int(k)};
        if (L.skillMore[k].contains(x, y)) return Hit{Part::More, Button::Close, int(k)};
        if (L.skillRows[k].contains(x, y)) return Hit{Part::Skill, Button::Close, int(k)};
    }
    return Hit{};
}

}  // namespace uagfx::merccard
