#pragma once

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::worlddim {

// 3.0.11-beta.49 (the concept's open windows: «Мир под окнами приглушён (отдельный переключатель)»): [interface]
// dim_world - while «Речі», «Характеристики» or «Довідник» is open the world behind darkens (35 %, in and out over
// 160 ms): one black quad put where the world ends in the frame's record (its first interface draw), so the game's
// windows and words, the driver's panels and the windows themselves stay as they are. On the game's thread.
void frame(bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record);
bool windowsOpen();  // one of those windows on the screen now
bool active();       // [interface] dim_world on and one of them open (the character block goes compact then)

}  // namespace uagfx::win::worlddim
