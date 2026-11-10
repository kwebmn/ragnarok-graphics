#include "core/switch_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::switchcard {
namespace {

// At scale 1: the card 380 wide, 18 in; its title 30, its line 20; a switch's row 30 (the switch 40 x 22)
constexpr float kW = 380, kPad = 18, kTitleH = 30, kHintH = 20, kRowH = 30, kSwitchW = 40, kSwitchH = 22;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kText = 0xEEF2F8, kMuted = 0x9AA3B5;

}  // namespace

Layout layout(int targetW, int targetH, float scale, size_t count) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad;
    L.title = panel::Box{pad, pad, inner - 28 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 24 * s, pad + 3 * s, 24 * s, 24 * s};
    L.hint = panel::Box{pad, pad + kTitleH * s, inner, kHintH * s};
    float y = pad + kTitleH * s + kHintH * s + 8 * s;
    for (size_t i = 0; i < count; ++i) {
        L.labels.push_back(panel::Box{pad, y, inner - kSwitchW * s - 10 * s, kRowH * s});
        L.toggles.push_back(panel::Box{w - pad - kSwitchW * s, y + (kRowH - kSwitchH) * s / 2, kSwitchW * s,
                                       kSwitchH * s});
        y += kRowH * s + 4 * s;
    }
    y += pad - 4 * s;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (size_t i = 0; i < L.toggles.size(); ++i)
        if (L.toggles[i].contains(x, y) || L.labels[i].contains(x, y)) return Hit{Part::Toggle, int(i)};
    return Hit{};
}

std::vector<Words> words(const Layout& L, const std::vector<bool>& on, const Show& sh, const Texts& t) {
    (void)on;
    (void)sh;
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    if (!t.hint.empty()) out.push_back(Words{L.hint, t.hint, kMuted, 11.5f * s, false, Align::Left});
    for (size_t i = 0; i < L.labels.size() && i < t.labels.size(); ++i)
        out.push_back(Words{L.labels[i], t.labels[i], kText, 13 * s, false, Align::Left});
    return out;
}

}  // namespace uagfx::switchcard
