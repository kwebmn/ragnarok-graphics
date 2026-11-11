#include "core/macro_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::macrocard {
namespace {

// At scale 1: the card 440 wide, 18 in; its title 30, its line 20; a row 30 (the key's words 56, the field the rest)
constexpr float kW = 440, kPad = 18, kTitleH = 30, kHintH = 20, kRowH = 30, kKeyW = 56, kButtonH = 36;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000;

}  // namespace

std::string keyName(int i) { return "Alt+" + std::to_string((i + 1) % 10); }

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad;
    L.title = panel::Box{pad, pad, inner - 28 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 24 * s, pad + 3 * s, 24 * s, 24 * s};
    L.hint = panel::Box{pad, pad + kTitleH * s, inner, kHintH * s};
    float y = pad + kTitleH * s + kHintH * s + 8 * s;
    for (int i = 0; i < kFields; ++i) {
        L.keys.push_back(panel::Box{pad, y, kKeyW * s, kRowH * s - 4 * s});
        L.fields.push_back(panel::Box{pad + kKeyW * s + 6 * s, y, inner - kKeyW * s - 6 * s, kRowH * s - 4 * s});
        y += kRowH * s;
    }
    y += 8 * s;
    L.save = panel::Box{pad, y, inner, kButtonH * s};
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    if (L.save.contains(x, y)) return Hit{Part::Save, -1};
    for (size_t i = 0; i < L.fields.size(); ++i)
        if (L.fields[i].contains(x, y) || L.keys[i].contains(x, y)) return Hit{Part::Field, int(i)};
    return Hit{};
}

}  // namespace uagfx::macrocard
