#include "core/login_continue.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/class_art.hpp"
#include "core/login_card.hpp"
#include "core/ui_style.hpp"

namespace uagfx::logincontinue {
namespace {

// At scale 1 (the concept's picture): 720 x 268, the art its height square at the left, 28 in at the right; the
// heading 14, the name 32, the line 15; «Грати» 56 high; the links 14.5, 22 apart (closer when long)
constexpr float kW = 720, kH = 268, kIn = 28, kPlayH = 56, kLinkPx = 14.5f, kLinkGap = 22, kTriangle = 16;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kLink = 0x8CB8FF, kRed = 0xFF5A4E, kChosenWords = 0xF5E6BE, kKey = 0xB9A47A;

std::string capital(const std::string& s) {
    std::string out = s;
    if (!out.empty()) out[0] = char(std::toupper((unsigned char)out[0]));
    return out;
}

std::string fill(const std::string& pattern, const std::string& v) {
    const size_t at = pattern.find("{1}");
    return at == std::string::npos ? pattern + " " + v : pattern.substr(0, at) + v + pattern.substr(at + 3);
}

}  // namespace

Layout layout(int targetW, int targetH, float scale, const Texts& t) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.w = int(std::ceil(kW * s));
    L.h = int(std::ceil(kH * s));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    L.art = panel::Box{0, 0, kH * s, kH * s};
    const float x = (kH + kIn) * s, w = (kW - kH - 2 * kIn) * s;
    L.heading = panel::Box{x, 24 * s, w, 20 * s};
    L.name = panel::Box{x, 44 * s, w, 42 * s};
    L.line = panel::Box{x, 88 * s, w, 22 * s};
    L.play = panel::Box{x, 132 * s, w, kPlayH * s};
    const auto width = [&](const std::string& words, float extra) {
        return std::max(80 * s, panel::wordsWidth(words, kLinkPx * s, false) + 2 + extra);
    };
    const float a = width(t.otherChar, 0), b = width(t.change, kTriangle * s), c = width(t.create, 0);
    const float gap = std::clamp((w - a - b - c) / 2, 10 * s, kLinkGap * s);
    const float ly = 204 * s, lh = 24 * s;
    L.otherChar = panel::Box{x, ly, a, lh};
    L.change = panel::Box{x + a + gap, ly, b, lh};
    L.create = panel::Box{x + a + gap + b + gap, ly, c, lh};
    return L;
}

Part hitTest(const Layout& L, float x, float y) {
    if (L.play.contains(x, y)) return Part::Play;
    if (L.otherChar.contains(x, y)) return Part::OtherChar;
    if (L.change.contains(x, y)) return Part::Change;
    if (L.create.contains(x, y)) return Part::Create;
    return Part::None;
}

void paint(panel::Picture& out, const Layout& L, const Account& a, const Show& sh) {
    const float s = L.s;
    const panel::Box all{0, 0, float(L.w), float(L.h)};
    panel::roundRect(out, all, 14 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    // its picture: the class's portrait (3.0.11-beta.178), else the character's head on a gold-dark ground
    const panel::Box pic{1 * s, 1 * s, L.art.w - 1 * s, L.art.h - 2 * s};
    if (a.art) {
        classart::cover(out, pic, a.art->px, a.art->width, a.art->height, 13 * s, classart::kLeftCorners);
    } else {
        panel::roundRect(out, pic, 13 * s, 0x2A2416, 0.9f);
        panel::roundRect(out, panel::Box{pic.x + pic.w - 14 * s, pic.y, 14 * s, pic.h}, 0, 0x2A2416, 0.9f);
        if (a.head) classart::centre(out, pic, *a.head);
    }
    const bool hot = sh.hover == Part::Play;
    panel::roundRect(out, L.play, 9 * s, kGold, hot ? 0.36f : 0.26f, kGold, 0.9f, 1.0f * s);
    // ▾ after «Змінити акаунт»'s words
    logincard::paintTriangle(out, L.change.x + L.change.w - kTriangle * s / 2, L.change.y + L.change.h / 2 + 1 * s,
                             9 * s, sh.hover == Part::Change || sh.changeOpen ? kGold2 : kLink, 0.95f);
}

std::vector<Words> words(const Layout& L, const Account& a, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.heading, t.heading, kMuted, 14 * s, false, Align::Left});
    const bool hasChar = !a.name.empty();
    out.push_back(Words{L.name, hasChar ? a.name : a.login, kText, 32 * s, true, Align::Left});
    std::string line;
    if (a.busyWindow > 0) {  // 3.0.11-beta.178
        line = fill(t.busy, std::to_string(a.busyWindow));
    } else if (a.refused) {
        line = t.refused;
    } else if (hasChar) {
        line = a.job;
        if (a.level > 0) line += (line.empty() ? "" : " · ") + std::to_string(a.level);
        if (!a.map.empty()) line += (line.empty() ? "" : " · ") + capital(a.map);
        line += (line.empty() ? "" : " · ") + fill(t.loginWord, a.login);
    } else {
        line = t.noChar;
    }
    out.push_back(Words{L.line, line, a.busyWindow > 0 ? 0xF2C46B : a.refused ? kRed : kMuted, 15 * s, false,
                        Align::Left});
    const float pw = panel::wordsWidth(t.play, 19 * s, true), kw = panel::wordsWidth(t.enterKey, 12.5f * s, false);
    const float px = L.play.x + (L.play.w - (pw + (kw > 0 ? 10 * s + kw : 0))) / 2;
    out.push_back(Words{panel::Box{px, L.play.y, pw + 2, L.play.h}, t.play, kChosenWords, 19 * s, true, Align::Left});
    if (!t.enterKey.empty())
        out.push_back(Words{panel::Box{px + pw + 10 * s, L.play.y + panel::baselineDrop(19 * s, 12.5f * s), kw + 2,
                                       L.play.h},
                            t.enterKey, kKey, 12.5f * s, false, Align::Left});  // 3.0.11-beta.196: on «Грати»'s line
    const auto link = [&](const panel::Box& b, const std::string& words, Part p, float less) {
        out.push_back(Words{panel::Box{b.x, b.y, b.w - less, b.h}, words, sh.hover == p ? kGold2 : kLink,
                            kLinkPx * s, false, Align::Left});
    };
    link(L.otherChar, t.otherChar, Part::OtherChar, 0);
    link(L.change, t.change, Part::Change, kTriangle * s);
    link(L.create, t.create, Part::Create, 0);
    return out;
}

}  // namespace uagfx::logincontinue
