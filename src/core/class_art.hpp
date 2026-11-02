#pragma once
#include <cstdint>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::classart {

// 3.0.11-beta.178 (the login concept's cards): the classes' pictures - the site's 17 portraits (tools/class_art.py:
// 360 x 360 JPEG crops of uaro.kiev.ua's class art) in ddraw.dll as resources (src/win/class_art.rc); a transcendent
// class's own, its second class, baby and mounted ones with it; the novices and the first classes none (the card's
// head of the character then, win/head_picture)
const char* pictureFor(int jobId);     // "high-priest.jpg" or nullptr
int resourceOf(const char* picture);   // its RCDATA id in ddraw.dll, 0 none
// A portrait (ARGB, opaque) over `to` of `out`: as wide as the box, its upper part when taller, scaled smoothly; the
// corners `corners` names rounded `radius`
constexpr int kTopCorners = 1 | 2, kLeftCorners = 1 | 4;  // top left 1, top right 2, bottom left 4, bottom right 8
void cover(panel::Picture& out, const panel::Box& to, const std::vector<uint32_t>& argb, int width, int height,
           float radius, int corners);
// A picture (premultiplied) over the middle of `to`, as it is (the character's head where no portrait is)
void centre(panel::Picture& out, const panel::Box& to, const panel::Picture& pic);

}  // namespace uagfx::classart
