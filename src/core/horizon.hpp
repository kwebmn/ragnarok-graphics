#pragma once
#include <cstdint>
#include <string>

// 3.0.22-beta.11 (the owner 19.11: «я хочу панорамы горизонта в клиенте красивые», «я хочу чтобы ты нарисовал это на
// видеокарте, не генерировал с помощью ИИ готовое»; docs/superpowers/specs/2026-11-19-horizon-panoramas-design.md):
// the 3D view's horizon drawn by the card. At a region's entry the card ray-marches procedural land - this region's
// climate - into a 360° strip (colour and coverage, normal, the sun's shadow and the haze), a slice a frame; each
// frame the sky samples the strip and lights it by the night, the sun and the haze. Here: the climates (one per
// core/region_look region), the strip's geometry, what a bake depends on and the numbers the bake shader reads.
namespace uagfx::horizon {

enum class Land : uint8_t { Hills, Alps, Desert, Sea, Karst, Canyon, Waste };
struct Rgb {
    float r = 0, g = 0, b = 0;
};
// The land past the map: distances in world units from the map's middle, heights in units up, haze per unit
struct Climate {
    Land land = Land::Hills;
    uint32_t seed = 1;
    float hillAmp = 170.0f, hillFreq = 1.0f / 1400.0f;     // the near hills
    float mountAmp = 4600.0f, mountFreq = 1.0f / 7600.0f;  // the far mountains (mesas, islands, pillars by the land)
    float mountStart = 3000.0f, mountFull = 9000.0f;       // where they begin to rise and stand whole
    float snowLine = 1500.0f, treeLine = 1300.0f;
    float haze = 2.6e-5f;  // the air's thickness: 1 - exp(-distance x haze) of the haze's colour
    Rgb grass, forest, rock, snow, sand;
};
// region_look's names ("Prontera", "Glast Heim"...); "" or unknown -> the default (Prontera's)
Climate climateOf(const std::string& region);

// The strip: the full circle wide, «rise» (the sine of the elevation, as the sky's skyAlong uses it) from kRiseLow at
// the bottom row to kRiseHigh at the top
constexpr float kRiseLow = -0.05f, kRiseHigh = 0.35f;
struct Level {
    int width = 0, height = 0;  // the strip's texels
    int steps = 0;              // the ray's steps at most
    int shadowSteps = 0;        // toward the sun (0: no shadows)
    int slices = 0;             // frames a bake takes
};
// card: effect_groups::cardLevel - 1 (Direct3D 11 or under 4 GB) the weak level, 2 (4 GB) 4096 wide, 3 (8 GB) 8192
// (3.0.22-beta.13, R-VDN9YH: 4096 a texel some 5 pixels wide on a 4K screen); a few rows a frame
Level levelForCard(int card);
float uOf(float x, float z);  // [0,1): atan2(z, x) / 2pi wrapped - u 0 at the world's +x, 0.25 at +z
float riseOfRow(float v);     // v 0 the top row .. 1 the bottom -> rise
float rowOfRise(float rise);

// The 3D view looks at a horizon: the switch, the wide view on, the map outdoors and not hung in the sky
bool wanted(bool on, bool wideView, bool outdoors, bool hung);

// What a bake depends on: the region, the level, the map's size - a bake anew (step); the sun moved on over
// kRelightAngle - its shadows drawn again (3.0.22-beta.15); == compares the sun within 1e-3
constexpr float kRelightAngle = 0.0175f;  // a degree
struct Key {
    std::string region;
    float sun[3] = {0, 0, 0};  // the map's sun, the world's axes (y down)
    bool weak = false;
    float mapRadius = 0.0f;
    bool operator==(const Key& o) const;
};

// The bake shader's numbers, into the shared constants by this table (shaders.cpp `hz...` names):
//   [0-11]  r0-r11:       land, seed, mapRadius, hillAmp, hillFreq, mountAmp, mountFreq, mountStart, mountFull,
//                         snowLine, treeLine, haze
//   [12-19] hole*:        grass rgb, forest rgb, steps, shadowSteps
//   [20-23] fogColor:     rock rgb, the eye's height
//   [24-27] sourceRect:   snow rgb, 0
//   [28-31] sourceClamp:  the sun xyz (the world's axes, y down), 0
//   [32-35] twin*:        the strip's width, height, 0, 0
//   [36-37] jitter:       the rows drawn now, first and past the last
//   [38-43] projection:   sand rgb, 0, 0, 0
constexpr int kPackedFloats = 44;
constexpr float kEyeHeight = 40.0f;  // the bake's eye over the map's ground
void pack(const Climate& c, float mapRadius, const float sun[3], const Level& level, int row0, int row1,
          float out[kPackedFloats]);

// The bake's progress (the renderers' twin state): a new key or level starts it anew from the top row; a slice of
// rows a frame; ready after the last; faded in over kFadeMs once ready
struct Bake {
    Key key;
    Level level;
    int rowsDone = 0, frames = 0;  // frames: the first bake's
    bool started = false, ready = false;
    bool relighting = false;  // 3.0.22-beta.15: its shadows drawn again for the sun moved on, the strip shown
    uint64_t startedAt = 0, readyAt = 0;
};
struct Slice {
    int row0 = 0, row1 = 0;  // the rows to draw now; row1 <= row0: none
    bool restart = false;    // a new key or level: (re)made textures, drawn anew
    bool relight = false;    // 3.0.22-beta.13: the sun moved on - its shadows drawn again, the strip shown meanwhile
    bool last = false;       // this slice ends the bake
};
constexpr uint64_t kFadeMs = 1000;
Slice step(Bake& b, const Key& key, const Level& level, uint64_t now);
float fadeOf(const Bake& b, uint64_t now);

}  // namespace uagfx::horizon
