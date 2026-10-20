#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "core/game_camera.hpp"
#include "core/ground_heights.hpp"
#include "core/map_axes.hpp"
#include "core/rsw.hpp"

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
    int blob = -1;         // 2.6.15: its character's blob
    float weight = 1.0f;   // 2.6.15: how dark its light makes it (Cast::weight)
};

// The shadows of the layers that stand on a blob's feet - over them across, above them, its character's (2.6.3: the
// same seed) or else at their depth - each laid from the feet along `dir` (a unit direction on the screen; per blob
// from `dirs` when it has as many) for `length` of its height above them.
std::vector<ShadowQuad> cast(const std::vector<Blob>& blobs, const std::vector<Layer>& layers,
                             const std::vector<std::array<float, 2>>& dirs, const float dir[2], float length);

// 2.6.14: one of a character's shadows - its way on the screen (a unit), its length (of a layer's height above the
// feet) and how dark (0-1, the layer's alpha times it)
struct Cast {
    float dir[2];
    float length;
    float weight;
};

// Each blob's shadows as `casts` has them, one list a blob (none: none but the game's round one).
std::vector<ShadowQuad> cast(const std::vector<Blob>& blobs, const std::vector<Layer>& layers,
                             const std::vector<std::vector<Cast>>& casts);

// 2.6.14: a light of the map a shadow falls from - where it is in the world (up along -y), how far its light reaches
// (world units), its flame's seed (torch::seedAt; 0 a steady lamp).
struct Light {
    float x, y, z;
    float reach;
    uint32_t seed;
};

// A light about a character's feet: the way away from it along the ground (x, z, a unit), the shadow's length - the
// feet's distance from it over its height above them (a light above throws it short, one far off long), 0.35-2.2 -
// and how dark: whole within a third of its reach, none at the reach, a flame's trembling with it.
struct Lit {
    float awayX, awayZ;
    float length;
    float weight;
};

// 2.6.18: what stands between the map's lights and a character (R-QPYCS9: "a fire behind the wall, its shadow goes
// through the wall") - its ground (none: nothing does) and the axes that put its places into the game's world
struct Walls {
    const ground_heights::Ground* ground = nullptr;
    map::Axes axes;
};

// The two nearest lights within their reach and more than a unit off the feet, the nearest first; `flicker` the
// flames' amount (as the torches' light trembles), `time` seconds. 2.6.18: and that reach the character - no wall of
// `walls` between them and a point five units over the feet.
std::vector<Lit> litBy(const std::vector<Light>& lights, float feetX, float feetY, float feetZ, double time,
                       float flicker, const Walls& walls = {});

// 2.6.14: the map's lights a shadow falls from - its torches (map effect 47, and 690, 691, 696 the game does not
// show), each reaching 40 units and trembling by its place, and its lamps (`lamps`: those not on a torch), reaching
// as far as their light (0.45 of their range, 15-70), steady - in the game's world by the map's axes.
std::vector<Light> lightsOf(const rsw::World& map, const std::vector<rsw::Light>& lamps, const map::Axes& axes);

// 2.6.14: each blob's shadows, `base` long of a layer's height: under the ground from the two nearest of `lights`
// about its feet (litBy, turned to the screen by the projection and the camera backwards, `known`), none with no fire
// near; under the open sky the sun's way (`sun`, `sunLength`) by day - it outshines a torch - and the fires' as the
// night (0-1) darkens the world, the sun's fading with it: both at dusk, the fires' alone at night.
std::vector<std::vector<Cast>> castsFor(const std::vector<Blob>& blobs, const std::vector<Light>& lights,
                                        const camera::Projection& p, const float viewToWorld[12], bool known,
                                        const float sun[2], float sunLength, bool openSky, float night, float base,
                                        double time, float flicker, const Walls& walls = {});

// 2.6.15: how much of the game's round spot stays under each blob - all of it with `spot` (effects.
// actor_shadows_spot), else none under its own shadow - back only as that one all but fades (a third of its darkness
// and less, a torch's far edge: no jump), all where none falls.
std::vector<float> spotShares(const std::vector<ShadowQuad>& quads, size_t blobs, bool spot);

// 2.6.3 (shadow_ps does the same): the ground's view depth at game pixel (x, y) under a character whose feet stand at
// (feetX, feetY), feetZ deep - along `slope` (view units a pixel down the screen, from the scene's depth just below
// the feet) but never deeper than the flat ground through the feet (`up`: the world's up in view space; zero, none).
// The slope's depth fell on the wall below a ledge or on a post in front: the ground looked far deeper there and the
// shadow falling on it was cut - it blinked as a character walked along a ledge.
float groundDepth(const camera::Projection& p, const float up[3], float feetX, float feetY, float feetZ, float slope,
                  float x, float y);

}  // namespace uagfx::actor_shadows
