#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/equip_window.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::equip {

// 3.0.18-beta.96 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): «Спорядження» laid out by RmlUi - five slots
// a column each side of the middle (the portrait, name, job, levels and the battle numbers two a line, or the
// controller's list of what fits), the ammo at the middle's foot, the footer (the cards' note, the drag's or the
// controller's buttons). refit() puts its boxes into the window's core layout.
struct SlotV {
    int index = -1;
    Rml::String icon, label, name;
    bool worn = false, spill = false, hot = false, focus = false, fits = false;
    float op = 1, iconW = 0, iconH = 0;
    std::vector<int> dots;  // a card slot each: 1 a card sits there
};
struct StatV {
    Rml::String name, value;
};
struct PickV {
    int index = -1;
    Rml::String icon, name;
    bool hot = false, focus = false;
    float iconW = 0, iconH = 0;
};
struct PadV {
    Rml::String glyph, words;
    float w = 0, h = 0;
};
struct Model {
    // 3.0.18-beta.100: the window's scale in pixels as 12 of its own (its sizes in rem: the text size and «Речі»'s
    // size settings as before the live layout)
    float base = 12;
    Rml::String ground = "#00000000", title, hotkey, who, job, level, portrait, cards, drag, pickTitle, pickNone;
    bool closeHot = false, picking = false, pad = false;
    float portraitW = 0, portraitH = 0;
    std::vector<SlotV> left, right;
    SlotV ammo;
    std::vector<StatV> stats;
    std::vector<PickV> picks;
    std::vector<PadV> padItems;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
using Source = std::function<std::string(const panel::Picture*)>;
// The controller's button as a picture `h` high: its source, its width in `w`
using Glyph = std::function<std::string(int button, float h, float& w)>;
constexpr int kMostPicks = 8;  // the middle's rows (core equipwin's: (312 - 32) / 34)
void fill(Model& m, const equipwin::Data& d, const equipwin::Show& sh, const equipwin::Texts& t, float scale,
          const Source& src, const Glyph& glyph);
bool refit(Rml::ElementDocument* doc, equipwin::Layout& L);

}  // namespace uagfx::uikit::equip
