#pragma once
#include <cstdint>

#include "core/config.hpp"

namespace uagfx::light {

// 2.5.20: the light modes - over what the file asks while they last, given back when they end, in a running game:
// - super-light ([quality] super_light) for the weakest graphics cards: no effect and no AI picture, the world at half
//   the picture's size (Direct3D 11), no anisotropy, sharpening, MSAA, TAA or supersampling, and Direct3D 11's own -
//   the world's textures as their colour alone, a 16-bit world target and depth, mip levels without blending
//   (docs/research/super-light.md);
// - the battery's ([frames] battery_saver) while a laptop runs on its battery: battery_fps frames at most, the effects
//   at most the weak set's, the world at 75 % at most (Direct3D 11), the frame pacer sleeping through, 5 frames when
//   the game is minimised (docs/research/battery.md).
// Direct3D 11's own (Renderer::setLight); kPlainUpscale (2.5.22): the smaller world made larger by one bilinear tap,
// not its own sharpening (Catmull-Rom and CAS, 45 taps a pixel of the picture)
enum Flag : uint8_t { kFlatWorld = 1, kWorld16 = 2, kPointMips = 4, kPlainUpscale = 8 };

struct Overrides {
    bool superLight = false, battery = false;  // the modes on now
    int effectsLevel = 3;                      // the effects' sets at most: 0 none, 1 the weak set's; 3 as the file
    bool aiHidden = false;
    int worldScaleMax = 100, renderScaleMax = 200;  // %
    int fpsMax = 0;                                 // 0: as the file has it
    int minimisedFps = 15;                          // [frames] save_power's in the background
    bool noSharpen = false, noAnisotropy = false, noSupersampling = false;  // the last: MSAA, TAA, ui_native's
    bool pacerSleeps = false;
    uint8_t flags = 0;  // Flag
    bool operator==(const Overrides& o) const;
    bool operator!=(const Overrides& o) const { return !(*this == o); }
};
Overrides overridesOf(const Config& c, bool onBattery);
// The effects the light modes leave of the effective ones (effect_groups::effective of the file).
EffectSettings effectsOf(const EffectSettings& effective, const Overrides& o);
// [frames] fps_limit (0: none) under them.
int fpsLimit(int file, const Overrides& o);
// kFlatWorld: the mip level a world texture is drawn from - its last (its colour alone), or for one with transparent
// texels the last of 16 texels a side or more (leaves and grilles keep their shape); 0: none (no mip levels).
int flatLevel(int width, int height, int levels, bool transparent);

}  // namespace uagfx::light
