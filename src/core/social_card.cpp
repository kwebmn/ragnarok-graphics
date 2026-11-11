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

const Entry* chosenEntry(const std::vector<Entry>& entries, const Show& sh) { return chosenOf(entries, sh); }

std::vector<int> order(const std::vector<Entry>& entries, Tab t) {
    std::vector<int> out;
    for (int pass = 0; pass < 2; ++pass)
        for (size_t i = 0; i < entries.size(); ++i)
            if (t == Tab::Party ? pass == 0 : entries[i].online == (pass == 0)) out.push_back(int(i));
    return out;
}

int onlineCount(const std::vector<Entry>& entries) {
    int n = 0;
    for (const Entry& e : entries) n += e.online;
    return n;
}

std::vector<Item> menu(Tab t, const Entry& e, bool leader, bool elsewhere) {
    std::vector<Item> out;
    if (e.self) {
        if (t == Tab::Party) out.push_back(Item::Leave);
        return out;
    }
    if (e.online) out.push_back(Item::Whisper);
    if (t == Tab::Party) {
        if (leader) out.push_back(Item::Expel);
        out.push_back(Item::AddFriend);
        if (e.online && elsewhere) out.push_back(Item::Route);
    } else {
        out.push_back(Item::RemoveFriend);
    }
    out.push_back(Item::CopyName);
    return out;
}

}  // namespace uagfx::socialcard
