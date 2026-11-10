#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/shop_window.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::shop {

// 3.0.18-beta.99 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md, group 3): the NPC's shop laid out by RmlUi -
// the head (the NPC's name, the map, ×), the tabs «Купити» / «Продати» (the controller's LB / RB beside them), the
// list on the left (the search, its head, ten rows: icon, name, «→ n», price, weight), the cart on the right (six
// rows: icon, name, − amount +, sum, ×), the totals, the confirm with its key, «Очистити», the note, the footer's keys
// or the controller's buttons. refit() puts RmlUi's boxes into the window's core layout.
struct WareV {
    int index = -1;  // a place in the shown list
    Rml::String icon, name, mark, price, weight;
    bool used = false, hot = false, framed = false, inCart = false, unknown = false;
    float iconW = 0, iconH = 0;
};
struct LineV {
    int index = -1;  // the cart's row
    Rml::String icon, name, amount, sum;
    bool used = false, hot = false, framed = false, typed = false, caret = false;
    bool lessHot = false, moreHot = false, amountHot = false, dropHot = false;
    float iconW = 0, iconH = 0;
};
struct TotalV {
    Rml::String label, value, cls = "tv";
};
struct KeyV {
    Rml::String cap, words, glyph;  // glyph: the controller's picture ("" a key's cap)
    float w = 0, h = 0;
};
struct Model {
    Rml::String ground = "#00000000", title, place, buy, sell, query, hint, searchIcon, wareWord, priceWord;
    Rml::String weightWord, cartWord, amountWord, sumWord, empty, waiting, confirm, confirmKey, confirmGlyph, clear;
    Rml::String note, lbGlyph, rbGlyph;
    bool closeHot = false, buyOn = true, buyHot = false, sellHot = false, typing = false, caret = false;
    bool cartEmpty = true, isWaiting = false, ok = false, confirmHot = false, clearHot = false, clearOn = false;
    bool editing = false, pad = false;
    float glyphW = 0, glyphH = 0, lbW = 0, rbW = 0;
    std::vector<WareV> wares;
    std::vector<LineV> lines;
    std::vector<TotalV> totals;
    std::vector<KeyV> keys;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
using Source = std::function<std::string(const panel::Picture*)>;
using Glyph = std::function<std::string(int button, float h, float& w)>;
void fill(Model& m, const shopwin::Data& d, const std::vector<int>& shown, const shopwin::Show& sh,
          const shopwin::Texts& t, float scale, const Source& src, const Glyph& glyph);
bool refit(Rml::ElementDocument* doc, shopwin::Layout& L);

}  // namespace uagfx::uikit::shop
