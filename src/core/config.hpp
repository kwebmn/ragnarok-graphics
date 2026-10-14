#pragma once
#include <string>

namespace uagfx {

enum class LogLevel { Error = 0, Info = 1, Debug = 2 };
enum class Backend { DgVoodoo, Uaro };          // [renderer] backend
// [display] mode. Auto follows the game: a windowed game keeps its window (ROExt sizes it), full screen becomes
// borderless. Window and Borderless override what the game asks for.
enum class DisplayMode { Auto, Window, Borderless };
enum class UiFilter { Sharp, Smooth };  // [display] ui_filter: how magnified interface pictures are sampled

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
    // [quality] ai_textures: the world's textures enlarged x4 by a neural network on the GPU, cached in upscaled.grf
    // (experiment; it starts the 64-bit renderer); ai_budget: milliseconds of GPU time a frame for it, 1-20.
    bool aiTextures = false;
    int aiBudget = 2;
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
