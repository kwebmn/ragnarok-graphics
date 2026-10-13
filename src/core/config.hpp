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
    DisplayMode mode = DisplayMode::Auto;
    int monitor = 0;       // 0 = the main monitor
    int windowWidth = 0;   // window mode: inner size in pixels; 0 = the size the game asks for
    int windowHeight = 0;
    bool highResolution = true;  // draw at the window's real pixels under Windows scaling (stage 3); false = 0.2
    UiFilter uiFilter = UiFilter::Sharp;
    bool crispText = true;       // window text and lines at the window's real pixels when k > 1 (stage 6)
    int renderScale = 100;       // %, 100-200: the frame is drawn this much larger and shrunk when shown
    int sharpen = 50;            // 0-100: contrast adaptive sharpening of the picture on screen; 0 = off
    bool vsync = true;
    int fpsLimit = 0;      // 0 = no limit
    bool allowTearing = false;  // let G-SYNC / FreeSync monitors show frames as soon as they are ready
    LogLevel logLevel = LogLevel::Info;
};

Config parseConfig(const std::string& text);
const char* backendToken(Backend b);
const char* displayModeToken(DisplayMode m);
const char* uiFilterToken(UiFilter f);

}  // namespace uagfx
