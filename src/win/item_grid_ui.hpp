#pragma once
#include <map>
#include <string>
#include <vector>

#include "core/item_window.hpp"
#include "uikit/item_grid_live.hpp"
#include "win/uaui.hpp"

namespace uagfx::win::itemgridui {

// 3.0.18-beta.27 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the things' grid window
// («Речі», «Склад», the cart - core itemwin's) in RmlUi's markup: its glass, the title and ×, the tabs and the lit
// one's gold line, the search field (its icon, the caret), the cells (the icon, the count's and the refine's plates,
// «нове»), the scroll's thumb, the footer (the weight's bar, its fill by the tone), the drop's light, the uses'
// flashes, the tip, the words where core itemwin places them. Drawn only (passive): the window's clicks, drags, wheel
// and keys stay its own. One per window, on the game's thread.
// 3.0.18-beta.94 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): live - laid out by RmlUi (uikit/item_grid_live:
// the head, the tabs' flex row, the search, the cells wrapped eight a row, the footer, the tip beside the pointed
// cell); refit() puts its boxes into the window's layout each frame, so the window's own clicks, drags, drops, wheel,
// keys and the pad find the cells where they are drawn.
struct Ui {
    // Shown at (x, y) when uaUI is ready and `on`; `gen` another one: everything said again. False: not shown
    bool frame(const char* name, bool on, int x, int y, int gen, const itemwin::Layout& L,
               const std::vector<itemwin::Item>& items, const std::vector<int>& shown, const itemwin::Show& sh,
               const itemwin::Texts& t, const itemwin::Measure& m);
    // RmlUi's boxes (as laid out last) into the window's layout - its size, glass, tabs, search, cells, footer
    void refit(itemwin::Layout& L) const;
    void raise() {  // 3.0.18-beta.43: as uisheet::Sheet's (.44 its gold edge)
        doc.raise(10 + uaui::windowRank());
        doc.mark(uaui::windowMarked());
    }
    uaui::Doc doc;
    uikit::itemgrid::Model model;
    int gen = -1;
    std::map<std::string, panel::Picture> made;
    std::map<std::string, std::string> sources;
};

}  // namespace uagfx::win::itemgridui
