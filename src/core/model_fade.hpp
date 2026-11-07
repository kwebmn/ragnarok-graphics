#pragma once
#include <cstdint>

#include "core/game_camera.hpp"

namespace uagfx::modelfade {

// 3.0.13-beta.17 (the owner's pick 1 of «50 идей»: «Здания и деревья перед персонажем становятся прозрачными ...
// плавное затухание»): [effects] fade_models - a draw of the game's models (opaque, writing depth: TL1 x, y, z, rhw,
// colour at 16) that covers the character's body nearer than it goes see-through, back again when it no longer does

// The character on the screen: its feet (the camera's target, projected), its depth (view z) and its body's height in
// pixels (kBodyUnits tall at that depth)
struct Focus {
    bool known = false;
    float x = 0, y = 0, z = 0, h = 0;
    // 3.0.13-beta.20 (R-UQ7AGY: Prontera's fountain behind the character went see-through - the camera looks down, a
    // tall thing behind is nearer along its view): the projection, the camera, the camera's place and how far the
    // character is from it along the ground (the world's x, z; y is up) - a model covers only nearer along the ground
    camera::Projection p;
    float view[12] = {};
    float camX = 0, camZ = 0, ground = 0;
};
constexpr float kBodyUnits = 45.0f;  // a character sprite's height in the world's units, about
// a draw this much nearer than the feet covers the body; 3.0.13-beta.18 (R-TAA3SB: a wall right before the character
// and a roof over its feet stayed whole): 3, not 10 - the ground over the feet lies behind them anyway
constexpr float kNearer = 3.0f;
// view: the game's world -> view (c_j = sum_i w_i * view[3i+j] + view[9+j]); at: the camera's target in the world
Focus focusOf(const camera::Projection& p, const float view[12], const float at[3]);

// 3.0.13-beta.26 (the owner: «делай дыру, мигает» - betas .17-.25 chose the models over the body whole, here, and
// they went on and off as one walked and turned the camera): the hole round the character the renderer's shaders
// leave in the models before it (shaders.cpp seeThrough) - its feet x, y and body's height (game pixels), its depth,
// its distance from the camera along the ground, the world's up in the view (y, z: the game's camera never rolls)
// and the projection's f; all 0 when the focus is not known
void holeOf(const Focus& f, float out[8]);
// The share of a model's pixels left out at game pixel (gx, gy) of a game picture width x height, its view depth z:
// shaders.cpp seeThrough's, kept the same (0 none, kHoleMost in the middle)
constexpr float kHoleMost = 0.7f;
float holeAt(const float hole[8], float gx, float gy, float z, int width, int height);

}  // namespace uagfx::modelfade
