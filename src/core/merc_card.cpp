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

void paint(panel::Picture& out, const Layout& L, const Info& i, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 6 * s, kWhite, sh.hover == int(Button::Close) ? 0.14f : 0.06f);
    const auto bar = [&](const panel::Box& b, int now, int most, uint32_t rgb) {
        panel::roundRect(out, b, b.h / 2, kWhite, 0.08f);
        if (most > 0 && now > 0)
            panel::roundRect(out, panel::Box{b.x, b.y, b.w * std::min(1.0f, float(now) / float(most)), b.h}, b.h / 2,
                             rgb, 0.9f);
    };
    bar(L.hp, i.hp, i.maxHp, kHp);
    bar(L.sp, i.sp, i.maxSp, kSp);
    panel::roundRect(out, panel::Box{L.skills.x, L.skills.y + L.skills.h - 1 * s, L.skills.w, 1 * s}, 0, kGold,
                     0.45f);  // 3.0.11-beta.218: the list's heading underlined
    for (size_t k = 0; k < L.skillRows.size() && k < i.skills.size(); ++k) {  // 3.0.16-beta.31
        if (!i.skills[k].usable) continue;
        if (sh.hoverSkill == int(k)) panel::roundRect(out, L.skillRows[k], 6 * s, kWhite, 0.07f);
        if (sh.padRow == int(k))  // 3.0.16-beta.39
            panel::roundRect(out, L.skillRows[k], 6 * s, kWhite, 0.0f, kGold, 0.9f, 1.5f * s);
        if (i.skills[k].level > 1) {
            panel::roundRect(out, L.skillLess[k], 5 * s, kWhite, sh.hoverLess == int(k) ? 0.16f : 0.06f);
            panel::roundRect(out, L.skillMore[k], 5 * s, kWhite, sh.hoverMore == int(k) ? 0.16f : 0.06f);
        }
    }
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
    out.push_back(Words{L.skills, t.skills, kGold2, 12.5f * s, true, Align::Left});
    for (size_t k = 0; k < L.skillRows.size(); ++k) {
        if (k >= i.skills.size()) {
            if (i.skills.empty()) out.push_back(Words{L.skillRows[k], "\xE2\x80\x94", kMuted, 12 * s, false,
                                                      Align::Left});  // —
            continue;
        }
        const SkillRow& sk = i.skills[k];
        const panel::Box& r = L.skillRows[k];
        out.push_back(Words{panel::Box{r.x + 6 * s, r.y, L.skillLess[k].x - r.x - 10 * s, r.h}, sk.name, kText,
                            12 * s, false, Align::Left});
        // 3.0.16-beta.31: «Рів. 3/5» while it is used below its level, ‹ › about it (the game's own list's only)
        const int use = sk.usable ? companion::useOf(sk.use, sk.level) : sk.level;
        std::string lv = t.skillLevel;
        if (const size_t p = lv.find("{1}"); p != std::string::npos)
            lv.replace(p, 3, use < sk.level ? std::to_string(use) + "/" + std::to_string(sk.level)
                                            : std::to_string(sk.level));
        out.push_back(Words{L.skillLevel[k], lv, use < sk.level ? kGold2 : kMuted, 11.5f * s, false,
                            Align::Center});
        if (sk.usable && sk.level > 1) {
            out.push_back(Words{L.skillLess[k], "\xE2\x80\xB9", kText, 12 * s, true, Align::Center});  // ‹
            out.push_back(Words{L.skillMore[k], "\xE2\x80\xBA", kText, 12 * s, true, Align::Center});  // ›
        }
    }
    out.push_back(Words{L.dismiss, t.dismiss, kDanger, 11 * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::merccard
