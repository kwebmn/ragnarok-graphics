#include "core/menu_window.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::menuwin {
namespace {

// The card at scale 1: 320 wide, 16 in, the title's row 28 and 10 under it, a button 38 high, 8 apart
constexpr float kWide = 320, kPad = 16, kTitleH = 28, kTitleGap = 10, kButtonH = 38, kGap = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kDeadRed = 0xFF5A4E;

bool primary(Kind k, size_t i) { return k != Kind::Alive && i == 0; }  // the death's first: its way on

const std::string& wordsOf(const Texts& t, Action a) {
    switch (a) {
        case Action::Revive: return t.revive;
        case Action::SavePoint: return t.savePoint;
        case Action::CharSelect: return t.charSelect;
        case Action::Exit: return t.exit;
        case Action::Back: break;
    }
    return t.back;
}

}  // namespace

std::vector<Action> actions(Kind k) {
    switch (k) {
        case Kind::Alive: return {Action::CharSelect, Action::Exit, Action::Back};
        case Kind::Dead: return {Action::SavePoint, Action::CharSelect, Action::Exit, Action::Back};
        case Kind::DeadToken: break;
    }
    return {Action::Revive, Action::SavePoint, Action::CharSelect, Action::Exit, Action::Back};
}

int buttonId(Kind k, Action a) {
    if (a == Action::Back) return 0x77;
    if (k == Kind::Alive) return a == Action::CharSelect ? 0x9A : a == Action::Exit ? 0x9B : 0;
    switch (a) {
        case Action::SavePoint: return 0x9A;
        case Action::CharSelect: return 0xBA;
        case Action::Exit: return 0xB9;
        case Action::Revive: return k == Kind::DeadToken ? 0x11F : 0;
        case Action::Back: break;
    }
    return 0;
}

int firstFocus(Kind k) { return int(actions(k).size()) - 1; }

Layout layout(Kind k, int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const size_t n = actions(k).size();
    const float w = std::round(kWide * s), pad = std::round(kPad * s);
    float y = pad;
    L.title = panel::Box{pad, y, w - 2 * pad, std::round(kTitleH * s)};
    y += L.title.h + std::round(kTitleGap * s);
    for (size_t i = 0; i < n; ++i) {
        L.buttons.push_back(panel::Box{pad, y, w - 2 * pad, std::round(kButtonH * s)});
        y += std::round(kButtonH * s) + (i + 1 < n ? std::round(kGap * s) : 0.0f);
    }
    L.glass = panel::Box{0, 0, w, y + pad};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = std::max(0, (targetW - L.w) / 2);
    // under the middle (the character seen above it), kept in the screen
    const int below = targetH / 2 + int(std::lround(30 * s));
    L.y = std::max(0, std::min(below, targetH - L.h - int(std::lround(8 * s))));
    return L;
}

int buttonAt(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.buttons.size(); ++i)
        if (L.buttons[i].contains(x, y)) return int(i);
    return -1;
}

void paint(panel::Picture& out, const Layout& L, Kind k, const Show& sh, const Texts& t) {
    (void)t;
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, std::max(0.92f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    for (size_t i = 0; i < L.buttons.size(); ++i) {
        const panel::Box& b = L.buttons[i];
        const bool lit = int(i) == sh.hover || int(i) == sh.focus;
        if (primary(k, i))
            panel::roundRect(out, b, 8 * s, kGold, lit ? 0.26f : 0.16f, kGold2, lit ? 0.9f : 0.55f,
                             (lit ? 2.0f : 1.0f) * s);
        else
            panel::roundRect(out, b, 8 * s, kWhite, lit ? 0.12f : 0.05f, lit ? kGold2 : kWhite, lit ? 0.8f : 0.10f,
                             (lit ? 2.0f : 1.0f) * s);
    }
}

std::vector<Words> words(const Layout& L, Kind k, const Show& sh, const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s;
    const bool dead = k != Kind::Alive;
    out.push_back(Words{L.title, dead ? t.dead : t.menu, dead ? kDeadRed : kText, 17 * s, true, itemwin::Align::Left});
    const std::vector<Action> as = actions(k);
    for (size_t i = 0; i < L.buttons.size() && i < as.size(); ++i) {
        const panel::Box& b = L.buttons[i];
        // its key on its right: the focused one's Enter (the controller's A), «Повернутися до гри»'s Esc (B)
        std::string hint;
        if (as[i] == Action::Back) hint = t.esc;
        else if (int(i) == sh.focus) hint = t.enter;
        float hintW = hint.empty() ? 0 : m(hint, 12 * s, false) + 4 * s;
        if (hintW > 0 && m(wordsOf(t, as[i]), 14 * s, primary(k, i)) > b.w - 28 * s - hintW) {  // words first
            hint.clear();
            hintW = 0;
        }
        out.push_back(Words{panel::Box{b.x + 14 * s, b.y, b.w - 28 * s - hintW, b.h}, wordsOf(t, as[i]),
                            primary(k, i) ? kGold2 : kText, 14 * s, primary(k, i), itemwin::Align::Left});
        if (!hint.empty())
            out.push_back(Words{panel::Box{b.x + b.w - 14 * s - hintW, b.y, hintW, b.h}, hint, kMuted, 12 * s, false,
                                itemwin::Align::Right});
    }
    return out;
}

}  // namespace uagfx::menuwin
