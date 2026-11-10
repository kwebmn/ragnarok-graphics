#pragma once
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"
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
};
struct Icon {
    float x = 0, y = 0, w = 0, h = 0, op = 1;
    Rml::String src;
};
struct Word {
    float x = 0, y = 0, w = 0, h = 0, px = 12;
    Rml::String text, colour, cls;
};
struct Content {
    float w = 0, h = 0;
    Box glass;  // w 0: none
    Rml::String ground;
    std::vector<Box> under, over;
    std::vector<Icon> icons;
    std::vector<Word> words;

    // a box by the theme's class (none when it has no size)
    void add(std::vector<Box>& to, const panel::Box& b, float r, const char* cls, float edge = 0, float op = 1) {
        if (b.w > 0 && b.h > 0) to.push_back(Box{b.x, b.y, b.w, b.h, r, edge, op, cls});
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
            words.push_back(Word{x.box.x, x.box.y, x.box.w, x.box.h, x.px, x.text, colour,
                                 std::string(x.bold ? "word bold" : "word") + align});
        }
    }
};

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
    // The glass's ground: the window's colour at the windows' opacity
    static std::string ground(uint32_t rgb, float opacity);

private:
    uaui::Doc doc_;
    Content c_;
    int gen_ = -1;
    std::map<std::string, panel::Picture> made_;
    std::map<std::string, std::string> sources_;
};

}  // namespace uagfx::win::uisheet
