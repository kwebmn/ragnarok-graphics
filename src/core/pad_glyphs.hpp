#pragma once
#include <cstdint>
#include <string>

#include "core/window_panel.hpp"

namespace uagfx::padglyph {

// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): the controller's buttons as the interface
// shows them while the player plays with it - Xbox's A B X Y coloured letters in rings, PlayStation's cross, circle,
// square and triangle in theirs, the shoulders and triggers as pills with their names (LB, R2...), the D-pad's ways,
// the sticks as a ring in a ring. The grounds and shapes painted; the letters by GDI (letter()).

enum class Family : uint8_t { Xbox, Sony };
// uapad's kBindableButtons' order (A B X Y LB RB LT RT, the D-pad up right down left, L3 R3), then the fixed ones
enum class Button : uint8_t { A, B, X, Y, LB, RB, LT, RT, Up, Right, Down, Left, L3, R3, Start, Back, kCount };

Button buttonOf(uint16_t padBit);    // uapad's PadButton bit (kPadA 1 << 0 ... kPadRT 1 << 15)
float widthOf(Button b, float side);  // a pill wider than its height, the round ones `side`
void paint(panel::Picture& out, Button b, Family f, float x, float y, float side);
struct Letter {
    panel::Box box;
    std::string text;  // "" none (PlayStation's faces, the D-pad)
    uint32_t rgb = 0xECEFF5;
    float px = 0;
};
Letter letter(Button b, Family f, float x, float y, float side);

}  // namespace uagfx::padglyph
