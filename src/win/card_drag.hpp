#pragma once
#include <windows.h>

#include <algorithm>
#include <cmath>

#include "win/game_camera.hpp"

namespace uagfx::win {

// 3.0.11-beta.270 (the tester: «паті вікно та вікно налаштування паті - не перетягуються»): a card placed by its own
// layout moved by a press on its empty part - its shift from that place (the target's pixels, kept while the game
// runs); it follows the game's cursor while the button is held. begin() at the press (the target's point), apply()
// each frame to the layout's x and y (w, h its size) - kept inside the screen, a hand of it at least
struct CardDrag {
    float offX = 0, offY = 0, grabX = 0, grabY = 0;
    bool dragging = false;

    void begin(float tx, float ty) {
        dragging = true;
        grabX = tx - offX;
        grabY = ty - offY;
    }
    void end() { dragging = false; }
    void apply(int& x, int& y, int w, int h, int targetW, int targetH, double k, float hand) {
        int cx = 0, cy = 0;
        if (dragging) {
            const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
            if (!readGameCursor(cx, cy) || GetAsyncKeyState(button) >= 0) {
                dragging = false;
            } else {
                offX = float(cx) * float(k) - grabX;
                offY = float(cy) * float(k) - grabY;
            }
        }
        const float bx = float(x), by = float(y);
        offX = std::clamp(offX, hand - bx - float(w), float(targetW) - hand - bx);
        offY = std::clamp(offY, -by, float(targetH) - hand - by);
        x = int(std::lround(bx + offX));
        y = int(std::lround(by + offY));
        (void)h;
    }
};

}  // namespace uagfx::win
