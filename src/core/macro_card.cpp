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

void paint(panel::Picture& out, const Layout& L, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 6 * s, kWhite, sh.hoverClose ? 0.14f : 0.06f);
    for (size_t i = 0; i < L.fields.size(); ++i) {
        const bool focus = sh.focus == int(i), hot = sh.hover == int(i);
        panel::roundRect(out, L.fields[i], 7 * s, kBlack, focus ? 0.42f : hot ? 0.34f : 0.28f, focus ? kGold : kWhite,
                         focus ? 0.85f : 0.12f, 1.0f * s);
    }
    panel::roundRect(out, L.save, 8 * s, kGold, sh.hoverSave ? 0.34f : 0.22f, kGold, 0.8f, 1.0f * s);
}

std::vector<Words> words(const Layout& L, const std::vector<std::string>& texts, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    out.push_back(Words{L.hint, t.hint, kMuted, 11.5f * s, false, Align::Left});
    for (size_t i = 0; i < L.fields.size(); ++i) {
        out.push_back(Words{L.keys[i], keyName(int(i)), kGold2, 12 * s, true, Align::Left});
        const std::string text = i < texts.size() ? texts[i] : "";
        const bool focus = sh.focus == int(i);
        const panel::Box field{L.fields[i].x + 8 * s, L.fields[i].y, L.fields[i].w - 16 * s, L.fields[i].h};
        out.push_back(Words{field, text + (focus && sh.caret ? "|" : ""), kText, 12.5f * s, false, Align::Left});
    }
    out.push_back(Words{L.save, t.save, kText, 13.5f * s, true, Align::Center});
    return out;
}

}  // namespace uagfx::macrocard
