#pragma once
#include <cstdint>
#include <map>
#include <vector>

namespace uagfx::castlook {

// 3.0.15-beta.2 (the owner: «сделай бету и покажи» - the skills redraw after nostalRE's recipes, the casting first):
// [effects] new_casting. The game's casting (effects 12, 54-59 - core/effect_exe.hpp isCasting) is a few ring walls
// of 20 sides about the caster's feet (R-8X3RCC's record: each triangle a draw of its own, u round the ring, v 1 at
// a wall's foot and 0 at its top, one 64x64 flame texture, alpha 180, and a wall of alpha 60 that grows ~2000 pixels
// up the screen in two seconds). The new look keeps the game's walls and times: their tops held low (shorten), drawn
// as crisp streaks by the shaders (kRingId), and a magic circle on the ground under the caster (circleQuad, kCircleId).
constexpr uint16_t kRingId = 0xFFF1, kCircleId = 0xFFF2;  // DrawCmd::effectId of the new look's draws
// .6 (the owner on the casting at a ground skill's target: «надо чтобы как вид игры, но с наворотами нового»): a
// casting on no actor is one at a target - its walls as the game draws them, one circle under it; each of its rings
// had its own primitive's seed, so its own circle (R-H9XW7J: a double, crooked ring) - all of an effect's under one key
inline uint32_t keyOf(uint32_t owner, uint16_t id) { return owner ? owner : 0xFFFF0000u | id; }
constexpr float kBaseAlpha = 150;  // the walls at the feet (180) - not the tall one (60)
// .5 (the owner: «сам каст можно меньше сделать?»): the walls drawn kShrink of the game's about their middle, the
// circle kCircleScale of the game's feet walls (1.8 before), the flame at most kFlameHeight of the screen (0.09)
constexpr float kCircleScale = 1.25f;  // the circle's radius in the feet walls' radii
constexpr float kShrink = 0.75f;
constexpr float kFlameHeight = 0.065f;

struct Box {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    double z = 0, rhw = 0;
    int n = 0;
    // .4: the depth at the farthest foot (y0, the screen's top) and the nearest (y1) - the ground's plane, z and rhw
    // affine on the screen: the circle's near half no longer under the ground
    float zTop = 0, rhwTop = 0, zBottom = 0, rhwBottom = 0;
    uint32_t colour = 0;
};

class Looks {
public:
    void frame();  // a frame begun: the boxes seen in the last one asked about in this one
    // A casting draw (TL1, `stride` bytes a vertex, x y z rhw, diffuse at 16, u v at 24) of `owner` in place: its
    // walls' tops brought down to at most about `most` pixels over their feet; its feet walls' bottoms noted
    void shorten(uint8_t* v, uint32_t count, uint32_t stride, uint32_t owner, float most);
    // .6: only its feet walls noted (the casting at a ground skill's target: the game's own look, a circle under it)
    void note(const uint8_t* v, uint32_t count, uint32_t stride, uint32_t owner);
    // The circle under `owner` from the last frame's feet walls (once a frame): false when none or already given
    bool circleQuad(uint32_t owner, uint8_t out[4 * 32]);

private:
    struct Foot {
        float x, y, z, rhw;
    };
    std::map<uint64_t, Foot> feet_;  // the foot under a wall's top: owner and u
    std::map<uint32_t, Box> now_, last_;
    std::vector<uint32_t> given_;
};

}  // namespace uagfx::castlook
