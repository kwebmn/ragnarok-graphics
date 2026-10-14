#pragma once
#include <string>

namespace uagfx {

enum class LogLevel { Error = 0, Info = 1, Debug = 2 };
enum class Backend { DgVoodoo, Uaro };          // [renderer] backend
// [display] mode. Auto follows the game: a windowed game keeps its window (ROExt sizes it), full screen becomes
// borderless. Window and Borderless override what the game asks for.
enum class DisplayMode { Auto, Window, Borderless };
enum class UiFilter { Sharp, Smooth };  // [display] ui_filter: how magnified interface pictures are sampled

// [ai] (0.12; 0.11's [quality] ai_textures, ai_budget and ai_denoise are read when [ai] has no such key): the world's
// textures enlarged by a neural network on the GPU (Real-ESRGAN, experiment; it starts the 64-bit renderer) and kept
// in upscaled.grf next to uaRO.exe.
struct AiSettings {
    bool enabled = false;
    // What it works on, the kinds the driver tells apart by how the game draws them (core/ai/kinds.hpp).
    bool ground = true;
    bool objects = true;   // buildings, trees, water
    // The sprites (pixel art the network may change too much), told apart by their files' folders (0.16; 0.13-0.15's
    // one "sprites" key sets all four where none of them is in the file).
    bool monsters = false;  // and homunculi, mercenaries
    bool players = false;   // bodies, heads, headgear, weapons, shields
    bool npcs = false;
    bool items = false;     // on the ground
    bool sprites() const { return monsters || players || npcs || items; }
    bool effects = false;  // light and sparks
    bool icons = true;     // 0.14: item and skill icons in the game's windows (crisp text's twins)
    bool uiPieces = true;  // 0.15: with icons, the windows' small pieces too - buttons, tabs, slots (baked-in text)
    std::string model = "general";      // ground and objects: general (realistic) or anime (drawn)
    std::string spriteModel = "anime";  // sprites and effects
    int denoise = 25;  // % of the grain the general model smooths away (Real-ESRGAN's denoise strength)
    int scale = 4;     // the result 4 or 2 times the game's texture (2: a quarter of the video memory)
    int budget = 2;    // ms of GPU time a frame, 1-20
    int minSize = 16;  // the textures it takes: both sides from min_size to max_size, 16-1024 (0.17: 16, was 64)
    int maxSize = 512;
    bool ahead = true;  // a map's textures known from earlier visits are done at once, not when they come into view
    bool operator==(const AiSettings& o) const;
    bool operator!=(const AiSettings& o) const { return !(*this == o); }
};
// The page's sets: Recommended (the defaults: ground and objects), All (every kind), Custom (anything else).
enum class AiPreset { Recommended, All, Custom };
AiPreset aiPresetOf(const AiSettings& a);  // whatever `enabled` is
AiSettings aiPresetSettings(AiPreset p, bool enabled);  // Custom: the defaults
bool aiKind(const AiSettings& a, int kind);  // ai::Kind order: ground, objects, sprites, effects, icons
// The settings as one word for uaro-render64.exe's command line ("enabled=1;ground=1;...;ahead=1") and back.
std::string aiArgument(const AiSettings& a);
AiSettings parseAiArgument(const std::string& word);

// [effects] (0.18): the picture made more modern by the driver, drawn on top of what the game draws. They apply while
// the game runs; Pause switches them off with the other improvements.
struct EffectSettings {
    // 0.22.6: the map's baked light - torches' and lamps' pools, the shadows of houses and trees on the ground -
    // smooth instead of blotches and steps (the ground's coarse lightmap blurred within its cells). On by default.
    bool smoothLight = true;
    // 0.23: the maps' torches drawn anew (core/torch.hpp) - a flame that burns and sparks instead of the game's
    // magnified drop, a soft glow in the air around it instead of its red blotch, and its warm flickering light on
    // the ground and the walls. On by default.
    bool torch = true;
    bool torchFlame = true;     // 0.23.2: the new flame; else the game's own picture of it (the rest stays new)
    int torchBrightness = 100;  // %, 0-200 (0.23.2): how bright the new flame burns
    int torchGlow = 100;    // %, 0-200: the glow around the flame
    int torchLight = 100;   // %, 0-200: the light it throws around
    int torchFlicker = 50;  // %, 0-100: how much it flickers
    // The game's light and sparks - what it adds to the picture: skills, portals, lamps - shine with a soft glow
    // (bloom from those draws alone; the rest of the picture keeps its brightness).
    bool glow = false;
    int glowStrength = 100;  // %, 0-200: how bright the glow is
    int glowSize = 50;       // %, 0-100: how far it spreads
    int glowLight = 50;      // %, 0-100 (0.20): how much the light falls on what is around, in its colour
    // 0.20: soft shadows where surfaces meet - at the walls, in the corners of buildings, under trees - on the map
    // and its models (screen-space ambient occlusion); the characters and the effects keep their light.
    bool ao = false;
    int aoStrength = 60;  // %, 0-100: how dark
    int aoSize = 50;      // %, 0-100: how far from a corner
    // 0.19: the maps' water drawn anew - waves, the picture above it reflected, the sun's glints, the bed seen
    // through it and darker with depth, foam along the shore.
    // 0.21: the picture's colours (not the interface's): saturation and brightness 100 as the game, contrast too;
    // warmth 50 neutral, less colder, more warmer; a vignette darkens the corners.
    bool grade = false;
    int gradeSaturation = 115;  // %, 0-200
    int gradeContrast = 108;    // %, 50-150
    int gradeBrightness = 100;  // %, 50-150
    int gradeWarmth = 50;       // %, 0-100
    int gradeVignette = 25;     // %, 0-100
    // 0.22: depth of field - sharp around the character in the middle of the screen, the rest softer with the
    // distance in depth from it (tilt-shift: the map looks like a model).
    bool dof = false;
    int dofStrength = 60;  // %, 0-100: how soft the far and the near get
    int dofRange = 40;     // %, 0-100: how deep the sharp zone is
    bool water = false;
    int waterWaves = 50;        // %, 0-100: 0 a still mirror
    int waterReflections = 50;  // %, 0-100: how much the water reflects
    int waterClarity = 50;      // %, 0-100: how deep one sees into it
    bool operator==(const EffectSettings& o) const;
    bool operator!=(const EffectSettings& o) const { return !(*this == o); }
};

// uaro-graphics.ini. Unknown keys and values that do not parse leave the default in place.
struct Config {
    Backend backend = Backend::DgVoodoo;
    // Part 3 (test): [renderer] bridge = 1 draws in the 64-bit process uaro-render64.exe; bridge_present = auto,
    // window or composition (how its picture reaches the game's window); bridge_dpi_scale = 1 scales a composition
    // surface for Windows scaling of the game's window.
    bool bridge = false;
    std::string bridgePresent = "auto";
    bool bridgeDpiScale = true;
    // Part 4 (test): [renderer] remix = 1 draws through Direct3D 9's fixed function, with the world in world space:
    // RTX Remix when its d3d9.dll is next to uaRO.exe, else the system's Direct3D 9.
    bool remix = false;
    DisplayMode mode = DisplayMode::Auto;
    int monitor = 0;       // 0 = the main monitor
    int windowWidth = 0;   // window mode: inner size in pixels; 0 = the size the game asks for
    int windowHeight = 0;
    bool highResolution = true;  // draw at the window's real pixels under Windows scaling (stage 3); false = 0.2
    UiFilter uiFilter = UiFilter::Sharp;
    bool crispText = true;       // window text and lines at the window's real pixels when k > 1 (stage 6)
    int renderScale = 100;       // %, 100-200: the frame is drawn this much larger and shrunk when shown
    int sharpen = 50;            // 0-100: contrast adaptive sharpening of the picture on screen; 0 = off
    int msaa = 4;                // stage 4: samples per pixel at polygon edges - 0 (off), 2, 4 or 8
    bool alphaToCoverage = true; // stage 4, with MSAA: soft edges where alpha testing cuts textures (leaves, fences)
    int anisotropy = 16;         // stage 4: anisotropic filtering of the world's textures - 0 (off), 2, 4, 8 or 16
    AiSettings ai;
    EffectSettings effects;
    // Part 3: [quality] dlss = off, dlaa, quality, balanced, performance or ultra - NVIDIA DLSS in the 64-bit
    // renderer (it starts that renderer; MSAA is off then).
    std::string dlss = "off";
    // Test switches (not on a page): dlss_jitter, dlss_motion = normal or inverted - the sign conventions DLSS
    // expects, should the picture come out blurred (jitter) or smeared (motion).
    bool dlssJitterInverted = false;
    bool dlssMotionInverted = false;
    bool vsync = true;
    int fpsLimit = 0;      // 0 = no limit
    int frameGeneration = 0;  // part 3: [frames] frame_generation = off, 2x, 3x, 4x - DLSS frame generation (DLSS on)
    bool allowTearing = false;  // let G-SYNC / FreeSync monitors show frames as soon as they are ready
    LogLevel logLevel = LogLevel::Info;
};

Config parseConfig(const std::string& text);
const char* backendToken(Backend b);
const char* displayModeToken(DisplayMode m);
const char* uiFilterToken(UiFilter f);

}  // namespace uagfx
