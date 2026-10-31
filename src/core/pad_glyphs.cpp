#include "core/pad_glyphs.hpp"

#include "core/ui_icons.hpp"

namespace uagfx::padglyph {
namespace {

constexpr uint32_t kGround = 0x0E1117, kEdge = 0xB9C0CE, kText = 0xECEFF5;
constexpr uint32_t kXbox[4] = {0x4CC35A, 0xF25A4E, 0x4A90F0, 0xF3C33A};  // A B X Y
constexpr uint32_t kSony[4] = {0x7FA7F5, 0xF5707A, 0xE68BD0, 0x3FD3A8};  // cross circle square triangle
constexpr uiicons::Icon kSonyShape[4] = {uiicons::Icon::PadCross, uiicons::Icon::PadCircle,
                                         uiicons::Icon::PadSquare, uiicons::Icon::PadTriangle};

bool face(Button b) { return b <= Button::Y; }
bool pill(Button b) { return (b >= Button::LB && b <= Button::RT) || b == Button::Start || b == Button::Back; }
bool dpad(Button b) { return b >= Button::Up && b <= Button::Left; }

const char* pillName(Button b, Family f) {
    const bool sony = f == Family::Sony;
    switch (b) {
        case Button::LB: return sony ? "L1" : "LB";
        case Button::RB: return sony ? "R1" : "RB";
        case Button::LT: return sony ? "L2" : "LT";
        case Button::RT: return sony ? "R2" : "RT";
        case Button::Start: return sony ? "Options" : "Start";
        case Button::Back: return sony ? "Share" : "Back";
        default: return "";
    }
}

}  // namespace

Button buttonOf(uint16_t padBit) {
    // uapad::PadButton: A B X Y LB RB Back Start, the D-pad up down left right, L3 R3, LT RT
    static const Button kBits[16] = {Button::A,    Button::B,     Button::X,  Button::Y,    Button::LB,
                                     Button::RB,   Button::Back,  Button::Start, Button::Up, Button::Down,
                                     Button::Left, Button::Right, Button::L3, Button::R3,   Button::LT,
                                     Button::RT};
    for (int i = 0; i < 16; ++i)
        if (padBit == uint16_t(1u << i)) return kBits[i];
    return Button::kCount;
}

float widthOf(Button b, float side) {
    if (b == Button::Start || b == Button::Back) return side * 2.3f;
    return pill(b) ? side * 1.55f : side;
}

void paint(panel::Picture& out, Button b, Family f, float x, float y, float side) {
    if (b >= Button::kCount || side <= 0) return;
    const float k = side / 24.0f, w = widthOf(b, side);
    if (pill(b)) {
        panel::roundRect(out, panel::Box{x, y, w, side}, side * 0.3f, kGround, 0.94f, kEdge, 0.95f, 1.4f * k);
        return;
    }
    const size_t i = size_t(b) - size_t(Button::A);
    const uint32_t ring = face(b) ? (f == Family::Sony ? kSony[i] : kXbox[i]) : kEdge;
    panel::roundRect(out, panel::Box{x, y, side, side}, side / 2, kGround, 0.94f, ring, 1.0f, 1.6f * k);
    if (face(b) && f == Family::Sony)
        uiicons::draw(out, kSonyShape[i], x, y, side, ring, 1.0f, 2.4f);
    else if (dpad(b))
        uiicons::draw(out, uiicons::Icon(int(uiicons::Icon::PadUp) + int(b) - int(Button::Up)), x, y, side, kText, 1.0f,
                      2.2f);
    else if (b == Button::L3 || b == Button::R3)  // a stick: a ring in its ring
        panel::roundRect(out, panel::Box{x + side * 0.24f, y + side * 0.24f, side * 0.52f, side * 0.52f}, side * 0.26f,
                         0x1E232D, 1.0f, kEdge, 0.9f, 1.1f * k);
}

Letter letter(Button b, Family f, float x, float y, float side) {
    Letter l;
    l.box = panel::Box{x, y, widthOf(b, side), side};
    if (face(b) && f == Family::Xbox) {
        static const char* const kLetters[4] = {"A", "B", "X", "Y"};
        l.text = kLetters[size_t(b)];
        l.rgb = kXbox[size_t(b)];
        l.px = side * 0.56f;
    } else if (pill(b)) {
        l.text = pillName(b, f);
        l.rgb = kText;
        l.px = side * 0.46f;
    } else if (b == Button::L3 || b == Button::R3) {
        l.text = b == Button::L3 ? "L" : "R";
        l.px = side * 0.38f;
    }
    return l;
}

}  // namespace uagfx::padglyph
