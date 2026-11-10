#include "core/command_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::commandcard {
namespace {

// At scale 1: the card 520 wide, 18 in; its title 30 high, the tabs 30; a row 28 high, 4 apart, two columns 8 apart
constexpr float kW = 520, kPad = 18, kTitleH = 30, kTabH = 30, kRowH = 28, kGap = 4, kColumnGap = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kGreen = 0x4CAF7A;

}  // namespace

void markStates(std::vector<Entry>& entries, const std::vector<std::pair<int, int>>& states) {
    for (Entry& e : entries) {
        e.state = -1;
        for (const auto& [state, code] : states)
            if (code == e.code) {
                e.state = state != 0 ? 1 : 0;
                break;
            }
    }
}

std::string keyName(int vk) {
    if (vk < 'A' || vk > 'Z') return "";
    return std::string("Alt+") + char(vk);
}

Layout layout(size_t most, int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s;
    L.title = panel::Box{pad, pad, w - 2 * pad - 30 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 26 * s, pad + 2 * s, 26 * s, 26 * s};
    float y = pad + kTitleH * s + 4 * s;
    const float tw = (w - 2 * pad - 2 * kGap * s) / 3;
    for (int i = 0; i < 3; ++i) L.tabs.push_back(panel::Box{pad + float(i) * (tw + kGap * s), y, tw, kTabH * s});
    y += kTabH * s + 10 * s;
    const int rows = int((std::max<size_t>(most, 1) + kColumns - 1) / kColumns);
    const float cw = (w - 2 * pad - kColumnGap * s) / kColumns;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < kColumns; ++c)
            L.rows.push_back(panel::Box{pad + float(c) * (cw + kColumnGap * s), y + float(r) * (kRowH + kGap) * s, cw,
                                        kRowH * s});
    y += float(rows) * (kRowH + kGap) * s - kGap * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::clamp(int(float(targetH) * 0.30f) - L.h / 2, 0, std::max(0, targetH - L.h));
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, 0};
    for (size_t i = 0; i < L.tabs.size(); ++i)
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, int(i)};
    for (size_t i = 0; i < L.rows.size(); ++i)
        if (L.rows[i].contains(x, y)) return Hit{Part::Row, int(i)};
    return Hit{};
}

std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xE2\x9C\x95", sh.hoverClose ? kText : kMuted, 13 * s, false, Align::Center});  // ✕
    for (size_t i = 0; i < L.tabs.size(); ++i)
        out.push_back(Words{L.tabs[i], t.tabs[i], int(i) == int(sh.tab) ? kGold2 : kText, 12.5f * s,
                            int(i) == int(sh.tab), Align::Center});
    const bool keys = sh.tab == Tab::Windows;
    for (size_t i = 0; i < L.rows.size() && i < entries.size(); ++i) {
        const panel::Box& r = L.rows[i];
        const Entry& e = entries[i];
        const bool hot = int(i) == sh.hover;
        float left = r.x + 10 * s;
        if (keys) {  // the window's key first, then its name
            out.push_back(Words{panel::Box{left, r.y, 52 * s, r.h}, keyName(e.code), kGold2, 12 * s, true,
                                Align::Left});
            left += 56 * s;
        }
        const float right = e.state >= 0 ? r.x + r.w - 62 * s : r.x + r.w - 8 * s;
        out.push_back(Words{panel::Box{left, r.y, right - left, r.h}, e.text, hot ? kGold2 : kText, 12.5f * s, hot,
                            Align::Left});
        if (e.state >= 0)
            out.push_back(Words{panel::Box{r.x + r.w - 58 * s, r.y, 50 * s, r.h}, e.state > 0 ? t.on : t.off,
                                e.state > 0 ? kText : kMuted, 11 * s, e.state > 0, Align::Center});
    }
    return out;
}

}  // namespace uagfx::commandcard
