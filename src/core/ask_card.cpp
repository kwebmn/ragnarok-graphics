#include "core/ask_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::askcard {
namespace {

// At scale 1: the card 380 wide, 18 in; its title 30 high, the words 2 rows of 18, a field 34 high, buttons 38
constexpr float kW = 380, kPad = 18, kTitleH = 30, kMessageH = 38, kFieldH = 34, kButtonH = 38, kGap = 10;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kText = 0xEEF2F8, kMuted = 0x9AA3B5;
constexpr uint32_t kBlack = 0x000000;

}  // namespace

bool request(Kind k) { return k == Kind::Trade || k == Kind::Party || k == Kind::Guild || k == Kind::Friend; }

bool input(Kind k) { return k == Kind::Number || k == Kind::Text || k == Kind::Count; }

std::vector<Button> buttons(Kind k) {
    if (request(k)) return {Button::Accept, Button::Decline};
    if (k == Kind::Question) return {Button::Yes, Button::No};
    if (k == Kind::Choice || k == Kind::Count) return {Button::Ok, Button::Cancel};
    if (k == Kind::Wait) return {};
    return {Button::Ok};
}

int gameButton(Button b) {
    switch (b) {
        case Button::Decline:
        case Button::Cancel: return 0x77;
        case Button::Yes: return 0x79;
        case Button::No: return 0x7A;
        default: return 0x76;
    }
}

float messageWidth(float scale) { return (kW - 2 * kPad) * (scale > 0 ? scale : 1.0f); }

Layout layout(const Card& c, int targetW, int targetH, float scale, const Texts& t) {
    (void)t;
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s;
    L.title = panel::Box{pad, pad, w - 2 * pad, kTitleH * s};
    float y = pad + kTitleH * s;
    if (!c.message.empty()) {  // 3.0.11-beta.151: as many rows as it needs (19 a row; at least two)
        const float h = std::max(kMessageH, 19.0f * float(std::max(1, c.messageLines))) * s;
        L.message = panel::Box{pad, y, w - 2 * pad, h};
        y += h;
    }
    if (input(c.kind)) {
        y += 4 * s;
        L.field = panel::Box{pad, y, w - 2 * pad, kFieldH * s};
        y += kFieldH * s;
    }
    if (c.kind == Kind::Count) {  // 3.0.11-beta.155: one, half, all
        y += 8 * s;
        const float qw = (w - 2 * pad - 2 * kGap * s) / 3;
        for (int i = 0; i < 3; ++i) L.quick.push_back(panel::Box{pad + float(i) * (qw + kGap * s), y, qw, 28 * s});
        y += 28 * s;
    }
    y += kGap * s;
    const std::vector<Button> bs = buttons(c.kind);
    if (!bs.empty()) {
        const float bw = (w - 2 * pad - float(bs.size() - 1) * kGap * s) / float(bs.size());
        for (size_t i = 0; i < bs.size(); ++i)
            L.buttons.push_back(panel::Box{pad + float(i) * (bw + kGap * s), y, bw, kButtonH * s});
        y += kButtonH * s;
    }
    y += pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::clamp(int(float(targetH) * 0.30f) - L.h / 2, 0, std::max(0, targetH - L.h));
    return L;
}

std::string boxWords(const std::string& now, const std::string& words, bool boxUp, bool adds) {
    if (!boxUp) return words;
    return adds && !words.empty() ? now + "\n" + words : now;
}

int buttonAt(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.buttons.size(); ++i)
        if (L.buttons[i].contains(x, y)) return int(i);
    return -1;
}

int quickAt(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.quick.size(); ++i)
        if (L.quick[i].contains(x, y)) return int(i);
    return -1;
}

int quickValue(Quick q, int most) {
    const int m = std::max(1, most);
    switch (q) {
        case Quick::One: return std::min(10, m);  // 3.0.15-beta.7: ten (one before), the most when fewer
        case Quick::Half: return std::max(1, m / 2);
        case Quick::All: break;
    }
    return m;
}

}  // namespace uagfx::askcard
