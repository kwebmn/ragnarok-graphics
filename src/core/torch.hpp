#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/game_camera.hpp"

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

// 2.6.18: the game burns each map torch as two effects at once - the map's effect source (FUN_00652690) launches
// effect 47 anew every 125 ticks of 60 a second (its .rsw emit speed; 5347 of the maps' 5664 torches) and each lasts
// 250 (DAT_006d9100[47]): two flames over each other, each its own frame of the animation, and at each launch one or
// three for a moment - two lights on the wall, one or three every two seconds ("на стене огонь мигает раз в 1-2
// секунды"). One torch then, drawn as the two were (kGameEffects): its light, heat and flame as two of them gave.
constexpr float kGameEffects = 2.0f;
// Takes a flame the game drew into the frame's: one standing where it stands (its base and tip within a game pixel,
// its depth within 1 %) is the same torch - the brighter fade kept; else added while there are fewer than `room`.
// false: no room for it.
bool take(std::vector<Flame>& flames, const Flame& f, size_t room);

// 2.6.17: the game's flames of a frame as the torches' pass draws them anew, kept steady ("every couple of seconds the
// torch blinks"). Each torch is followed from frame to frame by its place on the screen and keeps the seed it was
// first given (by a seed cell's edge its place read a cell over now and then - the new flame jumped to another
// shape); one missing a moment (a frame of the game's animation without its flame, or one flameOf does not read)
// is kept as it last was for up to kKeepSeconds; its brightness follows the game's fade up at once and down only
// slowly (kFall a second) - but a torch first seen takes the game's own fade-in.
class Keeper {
public:
    static constexpr double kKeepSeconds = 0.35;
    static constexpr float kFall = 0.8f;
    // flames and their place seeds in; out each flame's own seed (a torch followed keeps its first), the alphas
    // smoothed, the torches missing a moment added at the end
    void update(std::vector<Flame>& flames, std::vector<uint32_t>& seeds, double now);
    void clear() { kept_.clear(); }

private:
    struct Kept {
        Flame flame;
        float shown = 0.0f;
        double seen = -1.0;
    };
    std::unordered_map<uint32_t, Kept> kept_;
    double last_ = -1.0;
};

// How bright a flame burns at time t (seconds): about 1, within 1 +- amount, smooth; each seed its own way.
float flicker(double t, uint32_t seed, float amount);
// 2.6.16: effects.torch_flicker (0-100 %) as flicker()'s amount - 50 % the calm flicker the torches had (0.175), 0
// steady, 100 % four times its swing (0.7): the slider's ends now seen (R-N3ZZ3F: 0-100 % moved the light on the
// walls between steady and 12 %).
float flickerAmount(int percent);
// A torch's seed from where it stands on the map (world x and z, ten units a cell): the same while the camera moves.
uint32_t seedAt(float x, float z);
// 2.6.17: the seed of the torch a flame the game drew belongs to - seedAt of its middle (0.38 of the way up) back
// from the screen into the world by the projection and the camera backwards; of its screen place when the camera is
// not known. Its flicker's and the keeper's.
uint32_t placeSeed(const Flame& f, const camera::Projection& p, const float viewToWorld[12], bool cameraKnown);

}  // namespace uagfx::torch
