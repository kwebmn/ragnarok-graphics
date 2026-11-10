#pragma once
#include <map>
#include <string>
#include <vector>

#include "core/item_window.hpp"
#include "win/uaui.hpp"

namespace uagfx::win::itemgridui {

// 3.0.18-beta.27 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the things' grid window
// («Речі», «Склад», the cart - core itemwin's) in RmlUi's markup: its glass, the title and ×, the tabs and the lit
// one's gold line, the search field (its icon, the caret), the cells (the icon, the count's and the refine's plates,
// «нове»), the scroll's thumb, the footer (the weight's bar, its fill by the tone), the drop's light, the uses'
// flashes, the tip, the words where core itemwin places them. Drawn only (passive): the window's clicks, drags, wheel
// and keys stay its own. One per window, on the game's thread.
struct Ui {
    // Shown at (x, y) when uaUI is ready and `on`; `gen` another one: everything said again (the window's picture
    // would have been painted again). False: the window draws its own picture
    bool frame(const char* name, bool on, int x, int y, int gen, const itemwin::Layout& L,
               const std::vector<itemwin::Item>& items, const std::vector<int>& shown, const itemwin::Show& sh,
               const itemwin::Texts& t, const itemwin::Measure& m);

    struct BoxV {
        float x = 0, y = 0, w = 0, h = 0, r = 0, e = 0, op = 1;
        Rml::String cls;
    };
    struct IconV {
        float x = 0, y = 0, w = 0, h = 0, op = 1;
        Rml::String src;
    };
    struct WordV {
        float x = 0, y = 0, w = 0, h = 0, px = 12;
        Rml::String text, colour, cls;
    };
    void raise() {  // 3.0.18-beta.43: as uisheet::Sheet's (.44 its gold edge)
        doc.raise(10 + uaui::windowRank());
        doc.mark(uaui::windowMarked());
    }
    uaui::Doc doc;
    float w = 0, h = 0;
    Rml::String ground;
    BoxV glass;
    std::vector<BoxV> under, over;
    std::vector<IconV> icons;
    std::vector<WordV> words;
    int gen = -1;
    std::map<std::string, panel::Picture> made;
    std::map<std::string, std::string> sources;
};

}  // namespace uagfx::win::itemgridui
