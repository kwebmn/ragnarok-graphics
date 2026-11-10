#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/ui_icons.hpp"
#include "core/window_panel.hpp"

namespace uagfx::sheet {

// 3.0.18-beta.36 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5): what a window's markup holds, as plain
// data a core part fills by hand next to its painter (win/ui_sheet turns it into RmlUi's elements) - boxes by their
// colours (as panel::roundRect), the driver's pictures at their own size, the interface's icons, the words; a part
// of them cut at `clip` (a scrolled body)
struct Box {
    panel::Box box;
    float r = 0, edgeW = 0;
    uint32_t fill = 0, edge = 0;
    float fillA = 0, edgeA = 0;
};
struct Image {
    const panel::Picture* picture = nullptr;
    float x = 0, y = 0, opacity = 1;
};
struct Glyph {
    uiicons::Icon icon = uiicons::Icon::Search;
    float x = 0, y = 0, side = 16;
    uint32_t rgb = 0xFFFFFF;
    float alpha = 1, stroke = 2;
};
enum class Align : uint8_t { Left, Right, Center };
struct Text {
    panel::Box box;
    std::string text;
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false, wrap = false;
    Align align = Align::Left;
};
struct Layer {
    std::vector<Box> boxes;
    std::vector<Image> images;
    std::vector<Glyph> glyphs;
};
struct Model {
    float w = 0, h = 0;
    Layer under, clipped, over;  // `clipped`: cut at `clip`
    panel::Box clip;
    panel::Box world;  // 3.0.18-beta.36: the reference's world map (a picture the window paints), w 0 none
    std::vector<Text> words;
};

void rect(Layer& to, const panel::Box& b, float r, uint32_t fill, float fillA, uint32_t edge = 0, float edgeA = 0,
          float edgeW = 0);
void circle(Layer& to, float cx, float cy, float r, uint32_t rgb, float a);
// a straight line `width` wide from (x0, y0) to (x1, y1) - along an axis only (others none)
void line(Layer& to, float x0, float y0, float x1, float y1, float width, uint32_t rgb, float a);

}  // namespace uagfx::sheet
