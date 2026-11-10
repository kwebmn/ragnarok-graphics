#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/skill_window.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::skills {

// 3.0.18-beta.97 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): «Навички» laid out by RmlUi - the head (title,
// «План», «Список» | «Дерево», ×), the tabs' flex row, the notice, «Наступна»'s choices, the body (the list's eight
// rows - icon, name and what it is, the use level's arrows or the plan's «−», the dots, the level, «+» - or the tree's
// view, and the side panel), the footer (the points, the drag's words, the controller's buttons or the plan's
// «Застосувати» / «Скинути»). The tree is a diagram: its links, nodes and their words keep core skillwin's places,
// laid in the view RmlUi placed (clipped by it); the side panel's lines keep theirs in the panel's box.
// refit() puts RmlUi's boxes into the window's core layout (the tree's moved by the view's shift).
struct WordV {
    float x = 0, y = 0, w = 0, h = 0, px = 12;
    Rml::String text, colour = "#00000000", cls = "w";
};
struct BoxV {
    float x = 0, y = 0, w = 0, h = 0, r = 0, e = 0;
    Rml::String fill = "#00000000", edge = "#00000000";
};
struct IconV {
    float x = 0, y = 0, w = 0, h = 0, op = 1;
    Rml::String src;
};
struct TabV {
    Rml::String name;
    bool on = false, hot = false;
};
struct RowV {
    int index = -1;
    bool used = false, hot = false, focus = false, locked = false, lockTop = false, last = false;
    Rml::String icon, name, info, nameCls = "nm", use, level;
    float op = 1, iconW = 0, iconH = 0;
    bool arrows = false, lessOn = false, moreOn = false, minus = false, minusHot = false, plus = false;
    bool plusHot = false, lessHot = false, moreHot = false, planned = false, learned = false;
    std::vector<int> dots;  // 2 learned, 1 planned, 0 not
};
struct PadV {
    Rml::String glyph, words;
    float w = 0, h = 0;
};
struct Model {
    // 3.0.18-beta.100: the window's scale in pixels as 12 of its own (its sizes in rem: the text size and «Речі»'s
    // size settings as before the live layout)
    float base = 12;
    Rml::String ground = "#00000000", title, hotkey, planWord, listWord, treeWord, notice, points, pointsCls = "pts";
    Rml::String drag, applyWord, resetWord;
    bool closeHot = false, planButton = false, plan = false, planHot = false, views = false, tree = false;
    bool listHot = false, treeHot = false, pad = false, applyOn = false, applyHot = false, resetHot = false;
    bool scrollable = false;
    float thumbTop = 0, thumbH = 0;
    float viewW = 0, viewH = 0, contentW = 0;  // the tree's view and the list's width
    std::vector<TabV> tabs, choices;
    std::vector<RowV> rows;
    std::vector<BoxV> links, nodes, bars;
    std::vector<IconV> nodeIcons;
    std::vector<WordV> treeWords, sideWords;
    std::vector<PadV> padItems;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
using Source = std::function<std::string(const panel::Picture*)>;
using Glyph = std::function<std::string(int button, float h, float& w)>;
// The model from the window's state; `L` its layout as now (refit or core's: the tree's and the side panel's places
// are taken against its view and side boxes)
void fill(Model& m, const skillwin::Layout& L, const skillwin::Data& d, const skillwin::Show& sh,
          const skillwin::Texts& t, const skillwin::Measure& measure, const Source& src, const Glyph& glyph);
bool refit(Rml::ElementDocument* doc, skillwin::Layout& L);

}  // namespace uagfx::uikit::skills
