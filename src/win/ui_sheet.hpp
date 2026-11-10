#pragma once
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/pad_glyphs.hpp"
#include "core/ui_sheet_model.hpp"
#include "core/ui_icons.hpp"
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
    std::vector<Box> topBoxes;    // 3.0.18-beta.45: boxes and words over everything (▾'s list over the card's words)
    std::vector<Word> topWords;
    // 3.0.18-beta.29: a scrolled view (the skills' tree) - its boxes and icons in the window's pixels, cut at `clip`;
    // drawn after `under`, before `icons`
    Box clip;
    std::vector<Box> clipped;
    std::vector<Icon> clippedIcons;
    std::vector<Word> clippedWords;  // 3.0.18-beta.41

    // a box by the theme's class (none when it has no size)
    void add(std::vector<Box>& to, const panel::Box& b, float r, const char* cls, float edge = 0, float op = 1) {
        if (b.w > 0 && b.h > 0) to.push_back(Box{b.x, b.y, b.w, b.h, r, edge, op, cls});
    }
    // a box by its colours, as panel::roundRect(fill, fillA, edge, edgeA, width); `opacity` the next ones' (all)
    float opacity = 1;
    void rect(std::vector<Box>& to, const panel::Box& b, float r, uint32_t fill, float fillA, uint32_t edge = 0,
              float edgeA = 0, float width = 0) {
        if (b.w <= 0 || b.h <= 0) return;
        // 3.0.18-beta.54: in the theme - its colours, corners and edges
        Box x{b.x, b.y, b.w, b.h, r * uaui::themeRadius(), edgeA > 0 ? width * uaui::themeEdge() : 0, opacity, "paint"};
        x.paint = true;
        x.bg = rgba(fill, fillA);
        x.bd = rgba(edge, edgeA);
        to.push_back(x);
    }
    static Rml::String rgba(uint32_t rgb, float a) {
        char b[16];
        const int alpha = a <= 0 ? 0 : a >= 1 ? 255 : int(a * 255 + 0.5f);
        std::snprintf(b, sizeof b, "#%06x%02x", unsigned(uaui::themed(rgb) & 0xFFFFFF), alpha);
        return b;
    }
    // the words of a window's core (any of its Words: box, text, rgb, px, bold, align)
    template <typename W>
    void say(const std::vector<W>& in, float dx = 0, float dy = 0) {  // dx, dy: the words' boxes moved
        sayInto(words, in, dx, dy);
    }
    template <typename W>
    void sayInto(std::vector<Word>& to, const std::vector<W>& in, float dx = 0, float dy = 0) {
        char colour[16];
        for (W x : in) {
            x.box.x += dx, x.box.y += dy;
            std::snprintf(colour, sizeof colour, "#%06x", unsigned(uaui::themed(x.rgb) & 0xFFFFFF));
            const char* align = x.align == decltype(x.align)::Right    ? " right"
                                : x.align == decltype(x.align)::Center ? " centre"
                                                                       : "";
            to.push_back(Word{x.box.x, x.box.y, x.box.w, x.box.h, x.px, x.box.h, x.text, colour,
                              std::string(x.bold ? "word bold" : "word") + align});
        }
    }
};

// 3.0.18-beta.31: the words a part builds for GDI (win/picture_draw's: wide, DT_ alignment, broken lines, the halo)
void sayGdi(Content& c, const std::vector<Words>& in);

class Sheet;
// 3.0.18-beta.36: a core part's markup data (core/ui_sheet_model) into the sheet - its layers, its pictures and icons
// (made once by the sheet), its words
void fromModel(Content& c, const sheet::Model& m, Sheet& sheet);

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
    // 3.0.18-beta.43: called from a window's draw (the stack's order): its document at its place in the stack
    void raise() {
        doc_.raise(10 + uaui::windowRank());
        doc_.mark(uaui::windowMarked());  // 3.0.18-beta.44: the stack's gold edge
    }
    // 3.0.18-beta.60 (the possibilities' stage 4): the window's clickable boxes (its own pixels) for the pad's walk -
    // `kind` its winstack::Win; called each frame after frame()
    void nav(int kind, std::vector<panel::Box> targets);
    bool anywhere = false;  // 3.0.18-beta.33: set before the first frame - shown outside the world too (the login's)
    bool world = false;     // 3.0.18-beta.38: set before the first frame - with the world's overlays, under the parts

    // the runtime's
    std::vector<panel::Box> targets_;
    int focus_ = -1, x_ = 0, y_ = 0;
    Box pf_;  // the pad's ring (w 0: none)
    void showFocus();
    uaui::Doc& doc() { return doc_; }

private:
    uaui::Doc doc_;
    Content c_;
    int gen_ = -1;
    std::map<std::string, panel::Picture> made_;
    std::map<std::string, std::string> sources_;
};

// 3.0.18-beta.60: the pad on a window that has no commands of its own - the D-pad walks the boxes its sheet gave
// (a gold ring on the one chosen), A clicks it as the mouse would; false: no sheet walks that window
enum class NavCommand { Up, Down, Left, Right, Press };
bool padNav(int kind, NavCommand c);
bool hasNav(int kind);

}  // namespace uagfx::win::uisheet
