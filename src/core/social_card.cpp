#include "core/social_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::socialcard {
namespace {

// At scale 1: the card 360 wide, 16 in; the tabs 30 high; the header 20; a row 26 (10 shown at most); buttons 32
constexpr float kW = 360, kPad = 16, kTabH = 30, kHeaderH = 20, kRowH = 26, kButtonH = 32, kGap = 6;
constexpr size_t kMostRows = 10;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kOnline = 0x4CC777, kOffline = 0x5C6370;

const Entry* chosenOf(const std::vector<Entry>& entries, const Show& sh) {
    return sh.selected >= 0 && sh.selected < int(entries.size()) ? &entries[size_t(sh.selected)] : nullptr;
}

}  // namespace

int tabParam(Tab t) { return t == Tab::Friends ? 1 : 2; }

int gameButton(Action a) {
    switch (a) {
        case Action::Whisper: return 0xa6;
        case Action::Remove: return 0xa9;
        case Action::Leave: return 0xaa;
        case Action::Settings: return 0xa8;
        case Action::Close: break;
    }
    return 0x87;
}

std::vector<Action> actions(Tab t) {
    if (t == Tab::Friends) return {Action::Whisper, Action::Remove, Action::Settings};
    return {Action::Whisper, Action::Remove, Action::Leave, Action::Settings};
}

bool allowed(Tab t, Action a, const Entry* chosen) {
    switch (a) {
        case Action::Whisper:
        case Action::Remove: return chosen && !chosen->self;
        case Action::Leave: return t == Tab::Party;
        case Action::Settings:
        case Action::Close: break;
    }
    return true;
}

Layout layout(int targetW, int targetH, float scale, Tab tab, size_t count) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad;
    const float tw = (inner - 30 * s - kGap * s) / 2;
    L.tabs[0] = panel::Box{pad, pad, tw, kTabH * s};
    L.tabs[1] = panel::Box{pad + tw + kGap * s, pad, tw, kTabH * s};
    L.close = panel::Box{w - pad - 24 * s, pad + 3 * s, 24 * s, 24 * s};
    float y = pad + kTabH * s + 6 * s;
    L.header = panel::Box{pad, y, inner, kHeaderH * s};
    y += kHeaderH * s + 4 * s;
    const size_t shown = std::min(count, kMostRows);
    L.list = panel::Box{pad, y, inner, kRowH * s * float(std::max<size_t>(shown, 3))};
    for (size_t i = 0; i < shown; ++i)
        L.rows.push_back(panel::Box{pad, y + float(i) * kRowH * s, inner, kRowH * s - 2 * s});
    y += L.list.h + 10 * s;
    const size_t n = actions(tab).size();
    const float bw = (inner - float(n - 1) * kGap * s) / float(n);
    for (size_t i = 0; i < n; ++i)
        L.buttons.push_back(panel::Box{pad + float(i) * (bw + kGap * s), y, bw, kButtonH * s});
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(int(float(targetW) * 0.66f), 0, std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.20f), 0, std::max(0, targetH - L.h));
    return L;
}

int mostScroll(const Layout& L, size_t count) { return std::max(0, int(count) - int(L.rows.size())); }

Hit hitTest(const Layout& L, int scroll, size_t count, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (int i = 0; i < 2; ++i)
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, i};
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const int row = int(i) + scroll;
        if (row >= int(count)) break;
        if (L.rows[i].contains(x, y)) return Hit{Part::Row, row};
    }
    for (size_t i = 0; i < L.buttons.size(); ++i)
        if (L.buttons[i].contains(x, y)) return Hit{Part::Button, int(i)};
    return Hit{};
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Entry>& entries, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 6 * s, kWhite, sh.hoverClose ? 0.14f : 0.06f);
    for (int i = 0; i < 2; ++i) {
        const bool on = int(sh.tab) == i, hot = sh.hoverTab == i;
        panel::roundRect(out, L.tabs[i], 8 * s, on ? kGold : kWhite, on ? 0.24f : hot ? 0.10f : 0.04f,
                         on ? kGold : kWhite, on ? 0.8f : 0.12f, 1.0f * s);
    }
    if (L.rows.empty()) panel::roundRect(out, L.list, 8 * s, 0x000000, 0.18f);
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= entries.size()) break;
        const Entry& e = entries[at];
        const bool chosen = int(at) == sh.selected, hot = int(at) == sh.hoverRow;
        panel::roundRect(out, L.rows[i], 7 * s, chosen ? kGold : kWhite, chosen ? 0.16f : hot ? 0.06f : 0.025f,
                         chosen ? kGold : kWhite, chosen ? 0.8f : 0.0f, 1.0f * s);
        const float d = 8 * s;
        panel::roundRect(out, panel::Box{L.rows[i].x + 8 * s, L.rows[i].y + (L.rows[i].h - d) / 2, d, d}, d / 2,
                         e.online ? kOnline : kOffline, 0.95f);
    }
    const std::vector<Action> acts = actions(sh.tab);
    const Entry* chosen = chosenOf(entries, sh);
    for (size_t i = 0; i < L.buttons.size() && i < acts.size(); ++i) {
        const bool can = allowed(sh.tab, acts[i], chosen), hot = sh.hoverButton == int(i) || sh.focus == int(i);
        panel::roundRect(out, L.buttons[i], 7 * s, kWhite, !can ? 0.02f : hot ? 0.13f : 0.06f,
                         sh.focus == int(i) ? kGold : kWhite, sh.focus == int(i) ? 0.95f : 0.14f,
                         (sh.focus == int(i) ? 1.5f : 1.0f) * s);
    }
}

std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    for (int i = 0; i < 2; ++i)
        out.push_back(Words{L.tabs[i], t.tabs[i], kText, 13 * s, int(sh.tab) == i, Align::Center});
    out.push_back(Words{L.close, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    out.push_back(Words{L.header, t.header, kMuted, 12 * s, false, Align::Left});
    if (L.rows.empty()) out.push_back(Words{L.list, t.empty, kMuted, 12.5f * s, false, Align::Center});
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= entries.size()) break;
        const Entry& e = entries[at];
        const panel::Box& r = L.rows[i];
        const panel::Box name{r.x + 22 * s, r.y, r.w * 0.55f - 22 * s, r.h};
        out.push_back(Words{name, e.name, e.online ? kText : kMuted, 13 * s, e.self, Align::Left});
        if (e.leader)
            out.push_back(Words{panel::Box{r.x + r.w * 0.55f - 18 * s, r.y, 16 * s, r.h}, "\xE2\x99\x9B", kGold2,
                                12 * s, false, Align::Center});  // ♛
        const panel::Box where{r.x + r.w * 0.55f, r.y, r.w * 0.45f - 8 * s, r.h};
        out.push_back(Words{where, e.online ? e.map : t.offline, kMuted, 11.5f * s, false, Align::Right});
    }
    const std::vector<Action> acts = actions(sh.tab);
    const Entry* chosen = chosenOf(entries, sh);
    for (size_t i = 0; i < L.buttons.size() && i < acts.size() && i < 4; ++i)
        out.push_back(Words{L.buttons[i], t.actions[i], allowed(sh.tab, acts[i], chosen) ? kText : kMuted, 12 * s,
                            false, Align::Center});
    return out;
}

const Entry* chosenEntry(const std::vector<Entry>& entries, const Show& sh) { return chosenOf(entries, sh); }

}  // namespace uagfx::socialcard
