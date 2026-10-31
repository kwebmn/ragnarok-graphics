#include "core/pad_assign.hpp"

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "core/ui_style.hpp"

namespace uagfx::padassign {
namespace {

constexpr float kW = 380, kPad = 16, kTitleH = 26, kWhatH = 24, kRowH = 34, kRowGap = 4, kHintsH = 34, kGlyph = 20;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xECEFF5;
constexpr uint32_t kMuted = 0x9AA3B5;

padglyph::Family family(bool sony) { return sony ? padglyph::Family::Sony : padglyph::Family::Xbox; }

}  // namespace

Layout layout(size_t rows, int centreX, int centreY, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    float y = kPad;
    L.title = panel::Box{kPad * s, y * s, (kW - 2 * kPad) * s, kTitleH * s};
    y += kTitleH;
    L.what = panel::Box{kPad * s, y * s, (kW - 2 * kPad) * s, kWhatH * s};
    y += kWhatH + 8;
    for (size_t i = 0; i < rows; ++i) {
        L.rows.push_back(panel::Box{kPad * s, y * s, (kW - 2 * kPad) * s, kRowH * s});
        y += kRowH + kRowGap;
    }
    y += 6;
    L.hints = panel::Box{kPad * s, y * s, (kW - 2 * kPad) * s, kHintsH * s};
    y += kHintsH + 8;
    L.glass = panel::Box{0, 0, kW * s, y * s};
    L.w = int(std::ceil(L.glass.w));
    L.h = int(std::ceil(L.glass.h));
    L.x = centreX - L.w / 2;
    L.y = centreY - L.h / 2;
    return L;
}

int hitTest(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.rows.size(); ++i)
        if (L.rows[i].contains(x, y)) return int(i);
    return -1;
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Row>& rows, int focus, bool sony) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 14 * s, kCard, std::max(0.92f, uistyle::panel()), kWhite, 0.10f, 1.0f * s);
    panel::roundRect(out, panel::Box{L.hints.x, L.hints.y - 4 * s, L.hints.w, 1 * s}, 0, kWhite, 0.09f);
    for (size_t i = 0; i < L.rows.size() && i < rows.size(); ++i) {
        const panel::Box& b = L.rows[i];
        const bool on = int(i) == focus;
        panel::roundRect(out, b, 9 * s, on ? kGold : kWhite, on ? 0.13f : 0.04f, on ? kGold2 : kWhite,
                         on ? 1.0f : 0.08f, (on ? 2.0f : 1.0f) * s);
        padglyph::paint(out, padglyph::Button(rows[i].button), family(sony), b.x + 8 * s, b.y + (b.h - kGlyph * s) / 2,
                        kGlyph * s);
    }
    const float y = L.hints.y + (L.hints.h - kGlyph * s) / 2;
    padglyph::paint(out, padglyph::Button::A, family(sony), L.hints.x, y, kGlyph * s);
    padglyph::paint(out, padglyph::Button::B, family(sony), L.hints.x + 140 * s, y, kGlyph * s);
}

std::vector<Words> words(const Layout& L, const std::vector<Row>& rows, const std::string& what,
                         const std::string& title, const std::string& put, const std::string& cancel,
                         const std::string& empty, bool sony) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, title, kMuted, 13 * s, false, Align::Left});
    out.push_back(Words{L.what, what, kGold2, 16 * s, true, Align::Left});
    for (size_t i = 0; i < L.rows.size() && i < rows.size(); ++i) {
        const panel::Box& b = L.rows[i];
        const padglyph::Button button = padglyph::Button(rows[i].button);
        const float gx = b.x + 8 * s, gy = b.y + (b.h - kGlyph * s) / 2;
        const padglyph::Letter l = padglyph::letter(button, family(sony), gx, gy, kGlyph * s);
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
        const float x = gx + padglyph::widthOf(button, kGlyph * s) + 10 * s;
        out.push_back(Words{panel::Box{x, b.y, 54 * s, b.h}, rows[i].slot, kMuted, 13 * s, true, Align::Left});
        out.push_back(Words{panel::Box{x + 58 * s, b.y, b.x + b.w - x - 66 * s, b.h},
                            rows[i].now.empty() ? empty : rows[i].now, rows[i].now.empty() ? kMuted : kText, 13.5f * s,
                            false, Align::Left});
    }
    const float y = L.hints.y + (L.hints.h - kGlyph * s) / 2;
    for (const padglyph::Letter& l : {padglyph::letter(padglyph::Button::A, family(sony), L.hints.x, y, kGlyph * s),
                                      padglyph::letter(padglyph::Button::B, family(sony), L.hints.x + 140 * s, y,
                                                       kGlyph * s)})
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
    out.push_back(Words{panel::Box{L.hints.x + 28 * s, L.hints.y, 108 * s, L.hints.h}, put, kText, 14 * s, false,
                        Align::Left});
    out.push_back(Words{panel::Box{L.hints.x + 168 * s, L.hints.y, 150 * s, L.hints.h}, cancel, kText, 14 * s, false,
                        Align::Left});
    return out;
}

}  // namespace uagfx::padassign
