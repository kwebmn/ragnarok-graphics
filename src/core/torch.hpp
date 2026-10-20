#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::torch {

// 0.23, the torches of uaRO's maps (map effect 47): the sprite effect\torch_01, two layers a frame (its .act) - a
// 64x64 glow (32-bit frame 0) the game blows up 7.5 to 8.9 times, turned and tinted yellow, and one of seven 128x128
// flame frames (1-7) at twice their size, the flame some 26 x 60 texels of it. Magnified that much on a large screen
// they are a red blotch with edges and a blurred drop; the driver draws them anew (win/gpu/torches.cpp), known by
// their textures (core/map_objects.hpp).

// Where the flame is in its frames (torch_01's frames 1-7: the base at row 103, the tip at rows 43-54, columns 52-78),
// in texels of the 128x128 frame.
constexpr float kFrameSide = 128.0f;
constexpr float kBaseX = 65.0f, kBaseY = 103.0f;
constexpr float kHeight = 58.0f;  // base to tip
constexpr float kHalfWidth = 14.0f;

// A corner of the quad the game draws a layer with: D3D7 pixels, its depth and RHW, the texture's coordinates.
struct Corner {
    float x, y, z, rhw, u, v;
};

// A flame the game drew, in the game's pixels.
struct Flame {
    float baseX = 0, baseY = 0;  // the middle of its base
    float upX = 0, upY = 0;      // from the base to the tip
    float halfWidth = 0;
    float z = 0, rhw = 0;  // the quad's
    float alpha = 1;       // the game's fade (the quad's vertex alpha, 1 when it shows the layer as the .act has it)
    float tint[3] = {1, 1, 1};  // 0.25: a torch the game does not show - red, green, purple; white: the fire's own
};

// The flame of a quad drawn with a flame frame - its texture coordinates span the frame (the game's sprite setting
// may halve it, its texture may be larger) - false when the corners do not map the frame onto the screen.
bool flameOf(const Corner q[4], Flame& out);
// 2.6.12: the flame of such a quad from its four vertices as the game wrote them (each x, y, z, rhw, the diffuse
// colour - its alpha the game's fade - the specular one, u, v; `stride` bytes apart): false when they map no flame
// frame or it has no depth.
bool flameOfQuad(const uint8_t* vertices, size_t stride, Flame& out);

// How bright a flame burns at time t (seconds): about 1, within 1 +- amount, smooth; each seed its own way.
float flicker(double t, uint32_t seed, float amount);
// 2.6.16: effects.torch_flicker (0-100 %) as flicker()'s amount - 50 % the calm flicker the torches had (0.175), 0
// steady, 100 % four times its swing (0.7): the slider's ends now seen (R-N3ZZ3F: 0-100 % moved the light on the
// walls between steady and 12 %).
float flickerAmount(int percent);
// A torch's seed from where it stands on the map (world x and z, ten units a cell): the same while the camera moves.
uint32_t seedAt(float x, float z);

}  // namespace uagfx::torch
