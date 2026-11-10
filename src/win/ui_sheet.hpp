#pragma once
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"
#include "win/picture_draw.hpp"
#include "win/uaui.hpp"

namespace uagfx::win::uisheet {

// 3.0.18-beta.28 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): a window or a card in
// RmlUi's markup as its own code lays it out - its glass, the boxes under the icons (cells, fields, buttons by the
// theme's classes: .sh .cell, .sh .btn.main ...), the icons, the boxes over them (plates, pills, the tip), the words
// where the window's core places them. Each window fills it by hand from its layout and state; drawn only (passive):
// the window's clicks, drags, wheel and keys stay its own. One per window, on the game's thread.
struct Box {
    float x = 0, y = 0, w = 0, h = 0, r = 0, e = 0, op = 1;
    Rml::String cls;
    // 3.0.18-beta.29: a box by its own colours (rect()) instead of a class - never empty (the hidden one's evaluated)
    bool paint = false;
    Rml::String bg = "#00000000", bd = "#00000000";
};
struct Icon {
    float x = 0, y = 0, w = 0, h = 0, op = 1;
    Rml::String src;
};
struct Word {
    float x = 0, y = 0, w = 0, h = 0, px = 12, lh = 0;  // lh: the line's height (the box's for one line)
    Rml::String text, colour, cls;
};
// 3.0.18-beta.32: an arc (<ua-ring>) - its box, `value` of the circle from `start` degrees clockwise (0 at 3 o'clock),
// `width` dp at its edge (a pie to the middle with `sector`), the colours #rrggbbaa
struct Ring {
    float x = 0, y = 0, size = 0, value = 1, start = 0, width = 4;
    bool sector = false;
    Rml::String fill = "#00000000", track = "#00000000";
};

struct Content {
    float w = 0, h = 0, op = 1;  // op: the whole one's opacity (3.0.18-beta.32: the quiet chat)
    Box glass;  // w 0: none
    Rml::String ground;
    std::vector<Box> under, over;
    std::vector<Icon> icons;
    std::vector<Word> words;
    std::vector<Ring> rings;  // drawn after `under`, before the clipped view and `icons`
    std::vector<Icon> top;    // 3.0.18-beta.33: pictures over everything (the login's ▾ list)
    // 3.0.18-beta.29: a scrolled view (the skills' tree) - its boxes and icons in the window's pixels, cut at `clip`;
    // drawn after `under`, before `icons`
    Box clip;
    std::vector<Box> clipped;
    std::vector<Icon> clippedIcons;

    // a box by the theme's class (none when it has no size)
    void add(std::vector<Box>& to, const panel::Box& b, float r, const char* cls, float edge = 0, float op = 1) {
        if (b.w > 0 && b.h > 0) to.push_back(Box{b.x, b.y, b.w, b.h, r, edge, op, cls});
    }
    // a box by its colours, as panel::roundRect(fill, fillA, edge, edgeA, width)
    void rect(std::vector<Box>& to, const panel::Box& b, float r, uint32_t fill, float fillA, uint32_t edge = 0,
              float edgeA = 0, float width = 0) {
        if (b.w <= 0 || b.h <= 0) return;
        Box x{b.x, b.y, b.w, b.h, r, edgeA > 0 ? width : 0, 1, "paint"};
        x.paint = true;
        x.bg = rgba(fill, fillA);
        x.bd = rgba(edge, edgeA);
        to.push_back(x);
    }
    static Rml::String rgba(uint32_t rgb, float a) {
        char b[16];
        const int alpha = a <= 0 ? 0 : a >= 1 ? 255 : int(a * 255 + 0.5f);
        std::snprintf(b, sizeof b, "#%06x%02x", unsigned(rgb & 0xFFFFFF), alpha);
        return b;
    }
    // the words of a window's core (any of its Words: box, text, rgb, px, bold, align)
    template <typename W>
    void say(const std::vector<W>& in) {
        char colour[16];
        for (const W& x : in) {
            std::snprintf(colour, sizeof colour, "#%06x", unsigned(x.rgb & 0xFFFFFF));
            const char* align = x.align == decltype(x.align)::Right    ? " right"
                                : x.align == decltype(x.align)::Center ? " centre"
                                                                       : "";
            words.push_back(Word{x.box.x, x.box.y, x.box.w, x.box.h, x.px, x.box.h, x.text, colour,
                                 std::string(x.bold ? "word bold" : "word") + align});
        }
    }
};

// 3.0.18-beta.31: the words a part builds for GDI (win/picture_draw's: wide, DT_ alignment, broken lines, the halo)
void sayGdi(Content& c, const std::vector<Words>& in);

class Sheet {
public:
    // Shown at (x, y) when uaUI is ready and `on`; `gen` another one: `fill` called and everything said again.
    // `window`: drawn with the windows (over the panels). False: the caller draws its own picture
    bool frame(const char* name, bool on, bool window, int x, int y, int gen,
               const std::function<void(Content&)>& fill);
    // A picture of the driver's as an icon's source (its texture again when its pixels change)
    std::string icon(const panel::Picture* p);
    // One made once by its key (`paint` into a w x h picture), as an icon's source
    std::string made(const std::string& key, int w, int h, const std::function<void(panel::Picture&)>& paint);
    // 3.0.18-beta.31: the controller's button `button` (padglyph::Button) `h` high at (x, y) as an icon
    void glyph(Content& c, int button, bool sony, float x, float y, float h);
    // The glass's ground: the window's colour at the windows' opacity
    static std::string ground(uint32_t rgb, float opacity);
    bool anywhere = false;  // 3.0.18-beta.33: set before the first frame - shown outside the world too (the login's)

private:
    uaui::Doc doc_;
    Content c_;
    int gen_ = -1;
    std::map<std::string, panel::Picture> made_;
    std::map<std::string, std::string> sources_;
};

}  // namespace uagfx::win::uisheet
