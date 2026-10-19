#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "core/game_camera.hpp"

namespace uagfx::actor_shadows {

// 0.32, the characters' shadows (win/gpu/actor_shadows.cpp): instead of the game's round shadow alone, each layer of a
// character's sprite (body, head, headgear, weapon...) laid on the ground from its feet along the light, the way a
// low sun or a torch nearby throws it. All on the screen, in game pixels: the sprites face the screen.

// The game's shadow under a character (uaRO.exe FUN_00547b80, known by the driver): its middle - the feet - its half
// width and height, its view depth; 2.6.3: the character's seed (effect_exe::ownerSeed, 0 unknown).
struct Blob {
    float x, y, halfW, halfH, viewZ;
    uint8_t owner = 0;
};

// A layer of a sprite drawn this frame: its corners and their texture coordinates, its alpha, the view depth of its
// top and its bottom (uaRO.exe tilts a sprite's depth with its height: 0.16 units a pixel), its texture; 2.6.3: the
// seed of the character it was drawn for (the last whose shadow the game asked for, effect_exe::Owner; 0 unknown).
struct Layer {
    float corner[4][2];
    float uv[4][2];
    float alpha;
    float topZ, bottomZ;
    uint32_t texture;
    uint8_t owner = 0;
};

// A layer's shadow: the same picture, its corners on the ground.
struct ShadowQuad {
    float corner[4][2];
    float uv[4][2];
    float alpha;
    float viewZ;         // the feet's
    float feetX, feetY;  // 0.39.1: where they stand on the screen
    uint32_t texture;
    bool owned = false;  // 2.6.3: known as its character's by its seed, not by its depth
};

// The shadows of the layers that stand on a blob's feet - over them across, above them, its character's (2.6.3: the
// same seed) or else at their depth - each laid from the feet along `dir` (a unit direction on the screen; per blob
// from `dirs` when it has as many) for `length` of its height above them.
std::vector<ShadowQuad> cast(const std::vector<Blob>& blobs, const std::vector<Layer>& layers,
                             const std::vector<std::array<float, 2>>& dirs, const float dir[2], float length);

// 2.6.3 (shadow_ps does the same): the ground's view depth at game pixel (x, y) under a character whose feet stand at
// (feetX, feetY), feetZ deep - along `slope` (view units a pixel down the screen, from the scene's depth just below
// the feet) but never deeper than the flat ground through the feet (`up`: the world's up in view space; zero, none).
// The slope's depth fell on the wall below a ledge or on a post in front: the ground looked far deeper there and the
// shadow falling on it was cut - it blinked as a character walked along a ledge.
float groundDepth(const camera::Projection& p, const float up[3], float feetX, float feetY, float feetZ, float slope,
                  float x, float y);

}  // namespace uagfx::actor_shadows
