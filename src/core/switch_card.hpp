#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::switchcard {

// 3.0.11-beta.168 (task 5 of the remaining windows): a card of switches in style A - its title, a row a switch (its
// words, its state - a click on either flips it), ✕; for the game's small windows of checkboxes (the whisper's
// settings, UI window 0x44, the first). The parts that need no Windows; the words by GDI (win/whisper_setup).
struct Texts {
    std::string title, hint;  // hint: a line under the title ("" none)
    std::vector<std::string> labels;
};
struct Show {
    int hover = -1, focus = -1;
    bool hoverClose = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, hint, close;
    std::vector<panel::Box> labels, toggles;
};
Layout layout(int targetW, int targetH, float scale, size_t count);
enum class Part : uint8_t { None, Toggle, Close };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
}  // namespace uagfx::switchcard
