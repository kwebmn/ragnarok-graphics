#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/item_window.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::itemgrid {

// 3.0.18-beta.94 (the owner: «начинай нонстоп» - the windows with drag and drop live, docs/superpowers/plans/
// 2026-11-10-uaui-live-drag.md): the things' grid («Речі», «Склад», «Візок») laid out by RmlUi - the head, the tabs
// as a flex row, the search field, the cells wrapped eight a row, the footer - its words and looks from the model.
// After the layout refit() puts RmlUi's boxes into the window's core layout (glass, title, ×, tabs, search, grid,
// cells, footer, bar): its hit test, drops, flashes and the pad's places then fall where the cells are drawn.
struct TabV {
    Rml::String name, fresh;
    bool on = false, hot = false;
};
struct CellV {
    int index = -1;            // of the shown, the scroll counted
    Rml::String hit = "grid";  // "cell" a thing's, "grid" an empty one
    Rml::String icon, count, refine, mark, markCls, fresh, star, rising, risingColour = "#00000000";
    bool hot = false, starOn = false;
    float op = 1, iconW = 0, iconH = 0, flash = 0, ring = 0, rise = 0;
};
struct Model {
    Rml::String ground = "#00000000", title, hotkey, query, hint, also, weightWord, weightText, zeny, tone = "green";
    bool closeHot = false, typing = false, caret = false, bar = true, dropping = false, scrollable = false, tip = false;
    float fill = 0, thumbTop = 0, thumbH = 0, tipX = 0, tipY = 0;
    Rml::String tip1, tip2, tip3, tip4, searchIcon;
    std::vector<TabV> tabs;
    std::vector<CellV> cells;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
// A picture's source for <img> ("" none)
using Source = std::function<std::string(const panel::Picture*)>;
// The model from the window's things; `L` the window's layout (refit: the flashes found by their cells)
void fill(Model& m, const itemwin::Layout& L, const std::vector<itemwin::Item>& items, const std::vector<int>& shown,
          const itemwin::Show& sh, const itemwin::Texts& t, const Source& src);
// RmlUi's boxes into `L` (in its pixels, the document's top left at L.x, L.y); false - not laid out yet (L kept)
bool refit(Rml::ElementDocument* doc, itemwin::Layout& L);
// The tip beside the pointed cell, inside the window (its own size as laid out last); the cell's box in `L`
void placeTip(Rml::ElementDocument* doc, Model& m, const itemwin::Layout& L, int cell);

}  // namespace uagfx::uikit::itemgrid
