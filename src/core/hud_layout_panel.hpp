#pragma once
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::hudlayout {

// 3.0.11-beta.75: «Розміщення інтерфейсу»'s pictures - its bar at the top middle (its title and hint, «Скинути все»,
// «Готово») and each part's frame (a gold edge over it, its name and size in a tag); the words by GDI (win/hud_edit)
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};

struct Bar {
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    float s = 1;
    panel::Box reset, done;
};
Bar barLayout(int targetW, float scale);
enum class BarPart : uint8_t { None, Reset, Done, Body };
BarPart barHit(const Bar& b, float x, float y);  // the picture's pixels
struct BarTexts {
    std::string title, hint, reset, done;
};
void paintBar(panel::Picture& out, const Bar& b, BarPart hover);
std::vector<Words> barWords(const Bar& b, const BarTexts& t);

enum class Look : uint8_t { Idle, Hover, Dragged };
// tagWidth: its tag's words' width (the target's pixels) - a dark tag under them
void paintFrame(panel::Picture& out, int w, int h, float scale, Look look, float tagWidth);
// 3.0.11-beta.77: the sizing grip at the corner of the part under the pointer
void paintGrip(panel::Picture& out, int side, float scale, bool hover);
// Its tag's words: the part's name, its size past 100 % («Мінікарта · 110 %»)
std::vector<Words> frameWords(int w, int h, float scale, const std::string& name, int percent);

}  // namespace uagfx::hudlayout
