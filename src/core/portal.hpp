#pragma once
#include <array>

#include "core/game_camera.hpp"

namespace uagfx::portal {

// 2.7.48, the new portal (the user's pick «Б» of the options page: «такой же волнами и объёмный, но лучше»): the
// game's warp zone (uaRO.exe FUN_005ce290, effect 61) drawn anew in the shaders (portal_ps). The game's own, at 60
// ticks a second: a ring of light every 28 ticks, each 80 ticks long, a wall 5 units tall standing on a circle of 14
// that closes in by 0.15 a tick, its alpha 200; a column of 15 round them; sparks every 10 ticks rising for 130.
constexpr float kTick = 1.0f / 60.0f;
constexpr float kRingEvery = 28.0f * kTick;
constexpr float kRingLife = 80.0f * kTick;
constexpr float kRingRadius = 14.0f;
constexpr float kRingShrink = 0.15f;  // a tick
constexpr float kRingHeight = 5.0f;
constexpr float kRingAlpha = 200.0f / 255.0f;
constexpr float kColumnRadius = 15.0f;
constexpr int kRings = 3;  // alive at once: 80 / 28

struct Ring {
    float radius = 0.0f;  // world units
    float alpha = 0.0f;   // 0-kRingAlpha
};

// The rings at time t (seconds), the newest first: each closes in from kRingRadius, comes in over its first eighth
// and goes over its last fifth (FUN_005ce290's fade from 0x198 - 0x198 / 5); one past its life is gone (alpha 0).
std::array<Ring, kRings> ringsAt(double t);

// The screen box (game pixels: x0, y0, x1, y1) round a portal standing at foot (the view) with the world's up (the
// view's): its column, a fifth wider than kColumnRadius, `height` tall; false when any of it is behind the camera.
bool screenBox(const float foot[3], const float up[3], const camera::Projection& p, float height, float box[4]);

// 2.7.49 (the tester on 2.7.48: «порталы не нарисовались» - the game's left out, the new one never drawn): whether a
// draw of the game's portal is left out - only while the new one is wanted and was drawn the frame before, so that
// there is never no portal at all.
inline bool hidesGame(bool wanted, bool drawnLastFrame) { return wanted && drawnLastFrame; }

}  // namespace uagfx::portal
