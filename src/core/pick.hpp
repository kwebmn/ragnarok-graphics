#pragma once
#include <functional>

#include "core/frame.hpp"

namespace uagfx::pick {

// 2.3, uaRO Studio (L2): what the game drew under the cursor - its draws are pre-transformed (TL) vertices, x and y
// at each vertex's start in the game's pixels.
bool inTriangle(float ax, float ay, float bx, float by, float cx, float cy, float x, float y);  // edges in
// The last of a frame's draws (the one on top) wanted whose triangles hold (x, y) - lists, strips and fans, indexed
// or not; points and lines never. -1 none.
int lastDrawAt(const FrameRecord& r, float x, float y, const std::function<bool(const DrawCmd&)>& wanted);

// 2.3.1: a pick's turn. The key is read at a present, after that frame's draws were played: the next frame's records
// are looked through (seen: each record's texture on top under the point, 0 none - a later record's wins), and its
// present writes what was found (due, once).
class Request {
public:
    void ask(float x, float y);
    bool looking() const { return asked_; }
    float x() const { return x_; }
    float y() const { return y_; }
    void seen(uint32_t hit);
    bool due(uint32_t& hit);

private:
    bool asked_ = false, searched_ = false;
    float x_ = 0, y_ = 0;
    uint32_t hit_ = 0;
};

}  // namespace uagfx::pick
