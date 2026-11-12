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
        case Action::Settings: return t.settings;
        case Action::Layout: return t.layout;
        case Action::Test: return t.test;
        case Action::Report: return t.report;
        case Action::Keys: return t.keys;
        case Action::Account: return t.account;
        case Action::Back: break;
    }
    return t.back;
}

}  // namespace

std::vector<Action> actions(Kind k) {
    switch (k) {
        case Kind::Alive:  // 3.0.18-beta.121: ours over the game's; .226: «Змінити акаунт» on the top
            return {Action::Account, Action::Settings, Action::Keys, Action::Layout, Action::Test, Action::Report,
                    Action::CharSelect, Action::Exit, Action::Back};
        case Kind::Dead: return {Action::SavePoint, Action::CharSelect, Action::Exit, Action::Back};
        case Kind::DeadToken: break;
    }
    return {Action::Revive, Action::SavePoint, Action::CharSelect, Action::Exit, Action::Back};
}

bool ours(Action a) {
    return a == Action::Settings || a == Action::Layout || a == Action::Test || a == Action::Report;
}

bool local(Action a) { return a == Action::Keys; }

int buttonId(Kind k, Action a) {
    if (ours(a)) return 0;
    if (a == Action::Back) return 0x77;
    if (k == Kind::Alive)
        return a == Action::CharSelect || a == Action::Account ? 0x9A : a == Action::Exit ? 0x9B : 0;
    switch (a) {
        case Action::SavePoint: return 0x9A;
        case Action::CharSelect: return 0xBA;
        case Action::Exit: return 0xB9;
        case Action::Revive: return k == Kind::DeadToken ? 0x11F : 0;
        default: break;
    }
    return 0;
}

int firstFocus(Kind k) { return int(actions(k).size()) - 1; }

Layout layout(Kind k, int targetW, int targetH, float scale, size_t recapLines) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const size_t n = actions(k).size();
    const bool recap = k != Kind::Alive && recapLines > 0;  // .277: wider for the hits' lines
    const float w = std::round((recap ? 420.0f : kWide) * s), pad = std::round(kPad * s);
    float y = pad;
    L.title = panel::Box{pad, y, w - 2 * pad, std::round(kTitleH * s)};
    y += L.title.h + std::round(kTitleGap * s);
    if (k != Kind::Alive && recapLines > 0) {  // .277: «Останні удари» and a line a hit
        L.recap = panel::Box{pad, y, w - 2 * pad, std::round((24 + 19 * float(recapLines) + 8) * s)};
        y += L.recap.h + std::round(kTitleGap * s);
    }
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

std::vector<Words> words(const Layout& L, Kind k, const Show& sh, const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s;
    const bool dead = k != Kind::Alive;
    out.push_back(Words{L.title, dead ? t.dead : t.menu, dead ? kDeadRed : kText, 17 * s, true, itemwin::Align::Left});
    if (L.recap.h > 0) {  // 3.0.11-beta.277: the last hits, the last first
        out.push_back(Words{panel::Box{L.recap.x + 10 * s, L.recap.y + 4 * s, L.recap.w - 20 * s, 20 * s},
                            t.recapTitle, kDeadRed, 12.5f * s, true, itemwin::Align::Left});
        for (size_t i = 0; i < t.recap.size(); ++i)
            out.push_back(Words{panel::Box{L.recap.x + 10 * s, L.recap.y + (24 + 19 * float(i)) * s,
                                           L.recap.w - 20 * s, 19 * s},
                                t.recap[i], i == 0 ? kText : kMuted, 12 * s, i == 0, itemwin::Align::Left});
    }
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

std::vector<KeyLine> keyLines(const std::string& text) {
    std::vector<KeyLine> out;
    size_t from = 0;
    while (from < text.size()) {
        size_t end = text.find('\n', from);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(from, end - from);
        from = end + 1;
        if (line.find_first_not_of(' ') == std::string::npos) continue;
        static const char* const kDashes[] = {" \xE2\x80\x94 ", " - "};  // « — », « - »
        size_t at = std::string::npos, len = 0;
        for (const char* d : kDashes) {
            const size_t p = line.find(d);
            if (p != std::string::npos && p < at) at = p, len = std::char_traits<char>::length(d);
        }
        if (at == std::string::npos)
            out.push_back(KeyLine{"", line});
        else
            out.push_back(KeyLine{line.substr(0, at), line.substr(at + len)});
    }
    return out;
}

namespace {
// `text` cut into lines of `width` at most, at its spaces (a word wider than it on a line of its own)
std::vector<std::string> wrapped(const std::string& text, float width, float px, bool bold, const Measure& m) {
    std::vector<std::string> out;
    std::string line;
    size_t from = 0;
    while (from <= text.size()) {
        size_t end = text.find(' ', from);
        if (end == std::string::npos) end = text.size();
        const std::string word = text.substr(from, end - from);
        from = end + 1;
        if (word.empty()) continue;
        const std::string longer = line.empty() ? word : line + " " + word;
        if (!line.empty() && m(longer, px, bold) > width) {
            out.push_back(line);
            line = word;
        } else {
            line = longer;
        }
    }
    if (!line.empty()) out.push_back(line);
    return out;
}
}  // namespace

KeysView keysView(int targetW, int targetH, float scale, const std::vector<KeyLine>& lines, const Texts& t,
                  const Show& sh, const Measure& m) {
    KeysView v;
    Layout& L = v.layout;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = std::round(560 * s), pad = std::round(kPad * s), inner = w - 2 * pad;
    const float keyPx = 13 * s, whatPx = 12.5f * s, rowH = std::round(19 * s), gap = std::round(6 * s);
    float keyW = 0;  // as wide as the widest keys, a third of the card at most (longer ones wrapped)
    for (const KeyLine& k : lines) keyW = std::max(keyW, m(k.keys, keyPx, true));
    keyW = std::min(std::ceil(keyW) + std::round(4 * s), std::round(inner * 0.34f));
    const float whatX = pad + keyW + std::round(14 * s), whatW = w - pad - whatX;
    float y = pad;
    L.title = panel::Box{pad, y, inner, std::round(kTitleH * s)};
    v.words.push_back(Words{L.title, t.keys, kText, 17 * s, true, itemwin::Align::Left});
    y += L.title.h + std::round(kTitleGap * s);
    for (const KeyLine& k : lines) {
        const bool whole = k.keys.empty();
        const std::vector<std::string> parts = wrapped(k.what, whole ? inner : whatW, whatPx, false, m);
        const std::vector<std::string> keys =
            whole ? std::vector<std::string>{} : wrapped(k.keys, keyW, keyPx, true, m);
        for (size_t i = 0; i < keys.size(); ++i)
            v.words.push_back(Words{panel::Box{pad, y + float(i) * rowH, keyW, rowH}, keys[i], kGold2, keyPx, true,
                                    itemwin::Align::Left});
        for (size_t i = 0; i < parts.size(); ++i)
            v.words.push_back(Words{panel::Box{whole ? pad : whatX, y + float(i) * rowH, whole ? inner : whatW, rowH},
                                    parts[i], kText, whatPx, false, itemwin::Align::Left});
        y += float(std::max<size_t>(1, std::max(parts.size(), keys.size()))) * rowH + gap;
    }
    y += std::round(kTitleGap * s) - gap;
    L.buttons.push_back(panel::Box{pad, y, inner, std::round(kButtonH * s)});
    const panel::Box& b = L.buttons[0];
    const std::string hint = t.esc;
    const float hintW = hint.empty() ? 0 : m(hint, 12 * s, false) + 4 * s;
    v.words.push_back(Words{panel::Box{b.x + 14 * s, b.y, b.w - 28 * s - hintW, b.h}, t.keysBack,
                            sh.focus == 0 ? kGold2 : kText, 14 * s, false, itemwin::Align::Left});
    if (hintW > 0)
        v.words.push_back(Words{panel::Box{b.x + b.w - 14 * s - hintW, b.y, hintW, b.h}, hint, kMuted, 12 * s, false,
                                itemwin::Align::Right});
    y += b.h + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, (targetW - L.w) / 2);
    const int below = targetH / 2 + int(std::lround(30 * s));
    L.y = std::max(0, std::min(below, targetH - L.h - int(std::lround(8 * s))));
    return v;
}

}  // namespace uagfx::menuwin
