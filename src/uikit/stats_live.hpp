#pragma once
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/stat_window.hpp"
#include "uikit/layered.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::stats {

// 3.0.18-beta.101 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md, group 3): «Характеристики» (Alt+A) live - its
// head laid out by RmlUi (the title, its key, the tabs «Персонаж» / «Планування» / «Довідка», the free points' chip,
// ×), its body a layered one (uikit/layered): the tables, formulas and plans keep core statwin's places; the rows,
// «+», the plan's − + ‹ › and chips, the class's arrows are elements of their own there.
struct TabV {
    Rml::String name;
    bool on = false, hot = false;
};
struct Model {
    float base = 12, width = 0;  // base: 12 x the window's scale; width: its core width in pixels
    Rml::String ground = "#00000000", title, hotkey, chip;
    bool closeHot = false, chipOn = false;
    std::vector<TabV> tabs;
    layered::Layer layer;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
void fill(Model& m, const statwin::Layout& L, const statwin::Data& d, const statwin::Show& sh,
          const statwin::Texts& t, const statwin::Measure& measure);
// RmlUi's head boxes and the body's shift into the window's core layout (as core statwin made it this frame)
bool refit(Rml::ElementDocument* doc, statwin::Layout& L);

}  // namespace uagfx::uikit::stats
