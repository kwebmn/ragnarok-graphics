#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/window_panel.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::layered {

// 3.0.18-beta.101 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md, group 3): a live window's body of dense
// tables and formulas (stats' three tabs, the guild's tabs) - its words, its highlights and its pressable places kept
// where the window's core layout puts them, in a body RmlUi places under the window's flex head; the pressable places
// are elements of their own (data-hit, data-index), the window's refit shifts its core boxes by where the body went.
struct WordV {
    float x = 0, y = 0, w = 0, h = 0, px = 12;
    Rml::String text, colour = "#00000000", cls = "w";
};
struct BoxV {
    float x = 0, y = 0, w = 0, h = 0, r = 0, e = 0;
    Rml::String fill = "#00000000", edge = "#00000000";
};
struct HitV {
    float x = 0, y = 0, w = 0, h = 0;
    int index = -1;
    Rml::String part;
};
struct IconV {  // 3.0.18-beta.106: a picture (a character's face)
    float x = 0, y = 0, w = 0, h = 0;
    Rml::String src;
};
struct Layer {
    float w = 0, h = 0;
    std::vector<BoxV> boxes;
    std::vector<HitV> hits;
    std::vector<WordV> words;
    std::vector<IconV> icons;
};

// The body's markup: a div of class «lay» (data-hit «body») holding the boxes, the places and the words
const char* rml();
void bind(Rml::DataModelConstructor& c, Layer& l);
std::string hexa(uint32_t rgb, float alpha);
enum class Align : uint8_t { Left, Right, Center };
// What a layer is filled with - in the window's pixels; `origin` the body's top left in them
struct Builder {
    Layer& layer;
    panel::Box origin;
    size_t hadBoxes = 0, hadHits = 0, hadWords = 0, hadIcons = 0;
    explicit Builder(Layer& l, const panel::Box& o);
    ~Builder();  // the lists never shorter than before (RmlUi reads them while they change)
    void box(const panel::Box& b, float radius, uint32_t fill, float fillA, uint32_t edge = 0, float edgeA = 0,
             float edgeW = 0);
    void hit(const char* part, int index, const panel::Box& b);
    void word(const panel::Box& b, const std::string& text, uint32_t rgb, float px, bool bold, Align a);
    void icon(const panel::Box& b, const std::string& src);
};
// 3.0.18-beta.104 (R-WH87QX: «на весь екран растянулась»): the layer's height from what it holds (its lowest box,
// place or word, `pad` under it) - never from the window as laid out (its edge's pixel made it grow each frame)
float contentHeight(const Layer& l, float pad);
// The body's place against the window's: (dx, dy) to add to the core boxes of the body; false - not laid out
bool bodyShift(Rml::ElementDocument* doc, const panel::Box& coreOrigin, float& dx, float& dy);

}  // namespace uagfx::uikit::layered
