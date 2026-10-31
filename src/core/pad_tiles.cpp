#include "core/pad_tiles.hpp"

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::padtiles {
namespace {

constexpr float kTileW = 112, kTileH = 100, kGap = 10, kPad = 18, kTitleH = 34, kHintsH = 36, kGlyph = 20;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xECEFF5;
constexpr uint32_t kMuted = 0x9AA3B5;
constexpr uiicons::Icon kIcons[kTiles] = {uiicons::Icon::Bag,     uiicons::Icon::Stats,    uiicons::Icon::Skill,
                                          uiicons::Icon::Equip,   uiicons::Icon::Friends,  uiicons::Icon::Location,
                                          uiicons::Icon::Quest,   uiicons::Icon::Gear,     uiicons::Icon::Gamepad};

}  // namespace

Layout layout(int centreX, int centreY, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float gridW = 3 * kTileW + 2 * kGap, gridH = 3 * kTileH + 2 * kGap;
    L.glass = panel::Box{0, 0, (gridW + 2 * kPad) * s, (kPad + kTitleH + gridH + 12 + kHintsH + 10) * s};
    L.title = panel::Box{kPad * s, kPad * s, gridW * s, kTitleH * s};
    for (int i = 0; i < kTiles; ++i)
        L.tiles.push_back(panel::Box{(kPad + float(i % 3) * (kTileW + kGap)) * s,
                                     (kPad + kTitleH + float(i / 3) * (kTileH + kGap)) * s, kTileW * s, kTileH * s});
    L.hints = panel::Box{kPad * s, (kPad + kTitleH + gridH + 12) * s, gridW * s, kHintsH * s};
    L.w = int(std::ceil(L.glass.w));
    L.h = int(std::ceil(L.glass.h));
    L.x = centreX - L.w / 2;
    L.y = centreY - L.h / 2;
    return L;
}

int hitTest(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.tiles.size(); ++i)
        if (L.tiles[i].contains(x, y)) return int(i);
    return -1;
}

int moved(int at, int dx, int dy) {
    const int col = ((at % 3) + dx + 3) % 3, row = ((at / 3) + dy + 3) % 3;
    return row * 3 + col;
}

void paint(panel::Picture& out, const Layout& L, int focus, bool sony) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 14 * s, kCard, std::max(0.9f, uistyle::panel()), kWhite, 0.10f, 1.0f * s);
    panel::roundRect(out, panel::Box{L.hints.x, L.hints.y - 6 * s, L.hints.w, 1 * s}, 0, kWhite, 0.09f);
    for (size_t i = 0; i < L.tiles.size(); ++i) {
        const panel::Box& b = L.tiles[i];
        const bool on = int(i) == focus;
        panel::roundRect(out, b, 12 * s, on ? kGold : kWhite, on ? 0.13f : 0.045f, on ? kGold2 : kWhite,
                         on ? 1.0f : 0.10f, (on ? 2.0f : 1.0f) * s);
        const float icon = 36 * s;
        uiicons::draw(out, kIcons[i], b.x + (b.w - icon) / 2, b.y + 16 * s, icon, on ? kGold2 : 0xC7CCD6, 1.0f, 1.8f);
    }
    const padglyph::Family f = sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    const float y = L.hints.y + (L.hints.h - kGlyph * s) / 2;
    padglyph::paint(out, padglyph::Button::A, f, L.hints.x, y, kGlyph * s);
    padglyph::paint(out, padglyph::Button::B, f, L.hints.x + 130 * s, y, kGlyph * s);
}

std::vector<Words> words(const Layout& L, const std::vector<std::string>& names, const std::string& title,
                         const std::string& open, const std::string& close, bool sony) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, title, kText, 20 * s, true, Align::Left});
    out.push_back(Words{L.title, "Start", kMuted, 13 * s, false, Align::Right});
    for (size_t i = 0; i < L.tiles.size() && i < names.size(); ++i) {
        const panel::Box& b = L.tiles[i];
        out.push_back(Words{panel::Box{b.x + 4 * s, b.y + 60 * s, b.w - 8 * s, 30 * s}, names[i], kText, 14 * s, false,
                            Align::Center});
    }
    const padglyph::Family f = sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    const float y = L.hints.y + (L.hints.h - kGlyph * s) / 2;
    for (const padglyph::Letter& l : {padglyph::letter(padglyph::Button::A, f, L.hints.x, y, kGlyph * s),
                                      padglyph::letter(padglyph::Button::B, f, L.hints.x + 130 * s, y, kGlyph * s)})
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
    out.push_back(Words{panel::Box{L.hints.x + 28 * s, L.hints.y, 98 * s, L.hints.h}, open, kText, 14.5f * s, false,
                        Align::Left});
    out.push_back(Words{panel::Box{L.hints.x + 158 * s, L.hints.y, 120 * s, L.hints.h}, close, kText, 14.5f * s,
                        false, Align::Left});
    return out;
}

}  // namespace uagfx::padtiles
