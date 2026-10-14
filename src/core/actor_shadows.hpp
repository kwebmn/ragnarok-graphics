#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace uagfx::actor_shadows {

// 0.32, the characters' shadows (win/gpu/actor_shadows.cpp): instead of the game's round shadow alone, each layer of a
// character's sprite (body, head, headgear, weapon...) laid on the ground from its feet along the light, the way a
// low sun or a torch nearby throws it. All on the screen, in game pixels: the sprites face the screen.

// The game's shadow under a character (uaRO.exe FUN_00547b80, known by the driver): its middle - the feet - its half
// width and height, its view depth.
struct Blob {
    float x, y, halfW, halfH, viewZ;
};

// A layer of a sprite drawn this frame: its corners and their texture coordinates, its alpha, view depth, texture.
struct Layer {
    float corner[4][2];
    float uv[4][2];
    float alpha;
    float viewZ;
    uint32_t texture;
};

// A layer's shadow: the same picture, its corners on the ground.
struct ShadowQuad {
    float corner[4][2];
    float uv[4][2];
    float alpha;
    float viewZ;  // the feet's
    uint32_t texture;
};

// The shadows of the layers that stand on a blob's feet - over them across, above them, at their depth - each laid
// from the feet along `dir` (a unit direction on the screen; per blob from `dirs` when it has as many) for `length`
// of its height above them.
std::vector<ShadowQuad> cast(const std::vector<Blob>& blobs, const std::vector<Layer>& layers,
                             const std::vector<std::array<float, 2>>& dirs, const float dir[2], float length);

}  // namespace uagfx::actor_shadows
