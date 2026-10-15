#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::xbr {

// 0.41: a picture four times larger by xBR (level 2 - Hyllian's pixel-art scaler; our own implementation, right angles
// kept): each pixel's corners follow the edges through them - diagonals and slopes of 1:2 come out smooth, straight
// lines, strokes one pixel thick and the corners of letters and boxes stay sharp. For the windows' buttons, frames
// and labels (crisp text), where the AI melts letters one pixel thick. argb: w x h, straight alpha; past its edges its
// edge pixels go on (a strip the game repeats meets its copies). Returns 4w x 4h.
std::vector<uint32_t> scale4(const uint32_t* argb, int w, int h);

}  // namespace uagfx::xbr
