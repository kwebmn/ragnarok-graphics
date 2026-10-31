#pragma once
#include <cstdint>

#include "core/frame.hpp"
#include "core/window_panel.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win {

// 3.0.11-beta.49 (the concept: «Окна — те же панели, только плотнее: тень только у окон»): a window's soft shadow
// (core/ui_style shadow) drawn just before the window - its picture made again when the window's size changes
struct WindowShadow {
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, w = 0, h = 0, spread = 0;
    float scale = 0;
    bool upload = false;
    // The window's glass at (x, y, w, h) of the target, its corners `radius`
    void draw(FrameRecord& record, gpu::Renderer& renderer, int x, int y, int w, int h, float radius, float scale,
              float k);
};

// 3.0.11-beta.108 (the concept «Концепция окон и инвентаря», question 6 «ок»): the top window's gold edge over its
// glass while two or more windows are shown (win/window_stack) - its picture made again when the size changes
struct WindowEdge {
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, w = 0, h = 0;
    float scale = 0;
    bool upload = false;
    void draw(FrameRecord& record, gpu::Renderer& renderer, int x, int y, int w, int h, float radius, float scale,
              float k);
};

}  // namespace uagfx::win
