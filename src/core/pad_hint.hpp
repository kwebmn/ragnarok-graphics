#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::padhint {

// 3.0.11-beta.92 (the concept «Контроллер в интерфейсе uaRO», stage 6): the controller's hint line in the interface's
// own style - a glass bar at the bottom middle, each part its words before, its buttons as the controller's glyphs, its
// words after («↑↓ вибір · A OK · B скасувати»). The parts that need no Windows; the words by GDI.

struct Part {
    std::string before;
    std::vector<int> buttons;  // padglyph::Button
    std::string words;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass;
    struct Placed {
        std::vector<panel::Box> glyphs;
        panel::Box before, words;
    };
    std::vector<Placed> parts;
};
// Its bottom middle at (centreX, bottom); `width` measures words (panel::wordsWidth)
Layout layout(const std::vector<Part>& parts, int centreX, int bottom, float scale);
void paint(panel::Picture& out, const Layout& L, const std::vector<Part>& parts, bool sony);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Part>& parts, bool sony);

}  // namespace uagfx::padhint
