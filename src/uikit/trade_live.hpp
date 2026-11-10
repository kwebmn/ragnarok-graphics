#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/trade_window.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::trade {

// 3.0.18-beta.95 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): «Обмін» laid out by RmlUi - the two sides
// (their heads with the «Готово» pill, five cells a row, the zeny row - yours a field), the warning, the three
// buttons, the hint, the tip beside the pointed cell. refit() puts RmlUi's boxes into the window's core layout.
struct CellV {
    int index = -1;  // 0-9 yours, 10-19 the partner's
    Rml::String icon, count, refine;
    bool thing = false, hot = false, late = false;
    float op = 1, iconW = 0, iconH = 0;
};
struct SideV {
    Rml::String name, zeny;
    bool them = false, ready = false, lateZeny = false, dropping = false;
    std::vector<CellV> cells;
};
struct ButtonV {
    Rml::String label;
    bool on = false, main = false, hot = false;
};
struct Model {
    Rml::String ground = "#00000000", title, readyWord, zenyWord, warn, hint, tip1, tip2;
    bool closeHot = false, typing = false, caret = false, tip = false;
    float tipX = 0, tipY = 0;
    std::vector<SideV> sides;
    std::vector<ButtonV> buttons;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
using Source = std::function<std::string(const panel::Picture*)>;
void fill(Model& m, const tradewin::Side& me, const tradewin::Side& them, const tradewin::Show& sh,
          const tradewin::Texts& t, const Source& src);
bool refit(Rml::ElementDocument* doc, tradewin::Layout& L);
void placeTip(Rml::ElementDocument* doc, Model& m, const tradewin::Layout& L, int cell);

}  // namespace uagfx::uikit::trade
