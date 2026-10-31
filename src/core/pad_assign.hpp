#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::padassign {

// 3.0.11-beta.91 (the concept «Контроллер в интерфейсе uaRO», stage 6): «Призначити» - X on an item in a window opens
// a list of the controller's buttons that use the skill bar's cells, each with its cell and what the cell holds now;
// the D-pad chooses, A puts the item into that cell, B cancels. The parts that need no Windows; the words by GDI.

struct Row {
    int button = 0;    // padglyph::Button (uapad::kBindableButtons' order)
    std::string slot;  // «F3», «F3 · 2»
    std::string now;   // what its cell holds; "" empty
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, what, hints;
    std::vector<panel::Box> rows;
};
Layout layout(size_t rows, int centreX, int centreY, float scale);
int hitTest(const Layout& L, float x, float y);
void paint(panel::Picture& out, const Layout& L, const std::vector<Row>& rows, int focus, bool sony);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
// `what` - the item or skill being put
std::vector<Words> words(const Layout& L, const std::vector<Row>& rows, const std::string& what,
                         const std::string& title, const std::string& put, const std::string& cancel,
                         const std::string& empty, bool sony);

}  // namespace uagfx::padassign
