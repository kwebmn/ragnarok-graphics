#include "core/pad_hint.hpp"

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "core/ui_style.hpp"

namespace uagfx::padhint {
namespace {

constexpr float kH = 38, kPad = 14, kGlyph = 20, kGap = 4, kPartGap = 20, kText = 14;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kInk = 0xECEFF5, kMuted = 0x9AA3B5;

padglyph::Family family(bool sony) { return sony ? padglyph::Family::Sony : padglyph::Family::Xbox; }

}  // namespace

Layout layout(const std::vector<Part>& parts, int centreX, int bottom, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    float x = kPad * s;
    const float gy = (kH - kGlyph) / 2 * s;
    for (const Part& p : parts) {
        Layout::Placed placed;
        if (!p.before.empty()) {
            const float w = panel::wordsWidth(p.before, kText * s, false) + 2 * s;
            placed.before = panel::Box{x, 0, w, kH * s};
            x += w + kGap * s;
        }
        for (int b : p.buttons) {
            const float w = padglyph::widthOf(padglyph::Button(b), kGlyph * s);
            placed.glyphs.push_back(panel::Box{x, gy, w, kGlyph * s});
            x += w + 3 * s;
        }
        if (!p.buttons.empty()) x += (kGap - 3) * s;
        if (!p.words.empty()) {
            const float w = panel::wordsWidth(p.words, kText * s, false) + 2 * s;
            placed.words = panel::Box{x, 0, w, kH * s};
            x += w;
        }
        L.parts.push_back(placed);
        x += kPartGap * s;
    }
    x += (kPad - kPartGap) * s;
    L.glass = panel::Box{0, 0, std::max(x, 60 * s), kH * s};
    L.w = int(std::ceil(L.glass.w));
    L.h = int(std::ceil(L.glass.h));
    L.x = centreX - L.w / 2;
    L.y = bottom - L.h;
    return L;
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Part>& parts, bool sony) {
    const float s = L.s;
    panel::roundRect(out, L.glass, L.glass.h / 2, kCard, std::max(0.88f, uistyle::panel()), kWhite, 0.10f, 1.0f * s);
    for (size_t i = 0; i < L.parts.size() && i < parts.size(); ++i)
        for (size_t g = 0; g < L.parts[i].glyphs.size() && g < parts[i].buttons.size(); ++g) {
            const panel::Box& b = L.parts[i].glyphs[g];
            padglyph::paint(out, padglyph::Button(parts[i].buttons[g]), family(sony), b.x, b.y, b.h);
        }
}

std::vector<Words> words(const Layout& L, const std::vector<Part>& parts, bool sony) {
    std::vector<Words> out;
    const float s = L.s;
    for (size_t i = 0; i < L.parts.size() && i < parts.size(); ++i) {
        const Layout::Placed& at = L.parts[i];
        if (!parts[i].before.empty())
            out.push_back(Words{at.before, parts[i].before, kMuted, kText * s, false, Align::Left});
        for (size_t g = 0; g < at.glyphs.size() && g < parts[i].buttons.size(); ++g) {
            const panel::Box& b = at.glyphs[g];
            const padglyph::Letter l =
                padglyph::letter(padglyph::Button(parts[i].buttons[g]), family(sony), b.x, b.y, b.h);
            if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
        }
        if (!parts[i].words.empty())
            out.push_back(Words{at.words, parts[i].words, kInk, kText * s, false, Align::Left});
    }
    return out;
}

}  // namespace uagfx::padhint
