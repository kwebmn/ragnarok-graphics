#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/reports.hpp"
#include "core/window_panel.hpp"

namespace uagfx::card {

// 3.0.9-beta.4 (the user: «делай все» - the support canvas' screen 2): the developer's answer in the game - a card at
// the top centre of the game's picture, over its interface: a dot of the status's colour, «Відповідь на звернення»
// and the code, the status in a chip on the right, the developer's words (two lines at most), «Відкрити ·
// Ctrl+Shift+F10», «Пізніше» and the time. Its shapes here, its words by GDI over them (win/answer_card), as the
// windows' panel's.
struct Widths {  // the words' widths at the card's scale, the picture's pixels (win/answer_card measures them)
    float chip = 0, open = 0, later = 0;
};
struct Geometry {
    int width = 0, height = 0;  // the picture's
    float scale = 1, radius = 0;
    int lines = 1;  // the developer's words: 1 or 2
    panel::Box accent, dot, title, chip, chipText, quote, open, openText, later, laterText, time;
};
constexpr float kWidth = 460;  // at scale 1, while the game's picture has room
Geometry geometry(float scale, int roomWidth, int lines, const Widths& w);
panel::Point position(const Geometry& g, int roomWidth);  // the top centre, 8 x the scale under the top

enum class Part : uint8_t { None, Body, Open, Later };  // Body: the card itself - it opens the answer too
Part hitTest(const Geometry& g, float x, float y);

uint32_t statusColour(reports::Status s);  // 0xRRGGBB: orange need_info, green fixed, blue the rest
void paint(panel::Picture& out, const Geometry& g, reports::Status s, Part hover);

// The developer's words (its line ends as spaces) in `most` lines that `fits` takes whole - a word cut only when it
// is wider than a line alone - and «…» at the end of the last when more are left
std::vector<std::string> fitLines(const std::string& text, size_t most,
                                  const std::function<bool(const std::string&)>& fits);

}  // namespace uagfx::card
