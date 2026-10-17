#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::new_trees {

// 1.10, the remaster's new trees (effects.new_trees): a crown the game draws as a few flat cards of leaves is drawn
// instead as a volume of small bunches of the same leaves - pieces of its own picture where it is leaves - each a
// card facing the camera at its place in the crown, lit as the crown's part it is (the side to the sun lighter, the
// inside and the bottom darker) and swaying in the wind. The trunk stays the game's.

// How much of each of an 8 x 8 grid of a leaves picture is leaves (0-255; alpha 0 is a hole) - where bunches are cut.
struct Coverage {
    uint8_t cell[64] = {};
    bool any = false;
};
Coverage coverageOf(const uint32_t* argb, int width, int height);

struct Bunch {
    float pos[3] = {0, 0, 0};  // its middle, the world
    float size = 1.0f;         // its card's side, world units
    float uv[4] = {0, 0, 1, 1};  // the piece of the picture: u0 v0 u1 v1
    float light = 1.0f;        // its brightness, 0.4-1.3
    float turn = 0.0f;         // the card's turn on the screen, radians
};

// The bunches of a crown whose leaves fill the box lo-hi (the world, up along -y) at time t (s); toSun a unit vector
// to the sun in the world, sunLight 0-1 (night 0). Placed by `seed` (Steady's; 0: by the crown's place).
void bunchesOf(const float lo[3], const float hi[3], const Coverage& cov, double t, const float toSun[3],
               float sunLight, std::vector<Bunch>& out, uint32_t seed = 0);

// 1.25.1: the crowns kept from frame to frame - read back from the game's picture each frame, a crown's box moves by
// a hair as the camera turns or the player walks, and a seed by its rounded place flipped now and then: the whole
// crown laid out anew in a blink. A crown found again (its texture, within 1.5 units) keeps its box and its seed.
class Steady {
public:
    // The crown's steady box (lo, hi changed to the kept one unless it moved by more than a unit) and its seed.
    uint32_t settle(uint32_t texture, float lo[3], float hi[3]);
    void endFrame();  // the crowns not seen for 120 frames forgotten
    size_t size() const { return kept_.size(); }

private:
    struct Kept {
        uint32_t texture = 0, seed = 0;
        float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
        int unseen = 0;
        bool used = false;  // matched this frame: two crowns of one tree each keep their own
    };
    std::vector<Kept> kept_;
};

}  // namespace uagfx::new_trees
