#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "core/frame.hpp"

namespace uagfx::pick {

// 2.3, uaRO Studio (L2): what the game drew under the cursor - its draws are pre-transformed (TL) vertices, x and y
// at each vertex's start in the game's pixels.
bool inTriangle(float ax, float ay, float bx, float by, float cx, float cy, float x, float y);  // edges in
// Every draw of a frame's record wanted whose triangles hold (x, y), the one on top first - lists, strips and fans,
// indexed or not; points and lines never.
std::vector<int> drawsAt(const FrameRecord& r, float x, float y, const std::function<bool(const DrawCmd&)>& wanted);

// 2.3.1: a pick's turn. The key is read at a present, after that frame's draws were played: the next frame's records
// are looked through, and its present gives what was found (due, once). The textures under the point are taken
// opaque ones first - the game draws the cursor's cell (grid.tga) blended over the ground right under the cursor - and
// the key pressed again at the same place soon after gives the next one down, round and round.
class Request {
public:
    void ask(float x, float y, uint64_t nowMs);
    bool looking() const { return asked_; }
    float x() const { return x_; }
    float y() const { return y_; }
    // A record of the frame: the textures under the point, the one on top first, of its draws drawn opaque and of
    // those blended over what is under them. A later record's lie over an earlier one's.
    void seen(const std::vector<uint32_t>& opaque, const std::vector<uint32_t>& blended);
    bool due(uint32_t& hit);  // 0: none of the AI's there

private:
    bool asked_ = false, searched_ = false, before_ = false;
    float x_ = 0, y_ = 0;
    uint64_t at_ = 0;
    int depth_ = 0;  // presses at the same place
    std::vector<uint32_t> opaque_, blended_;
};

}  // namespace uagfx::pick
