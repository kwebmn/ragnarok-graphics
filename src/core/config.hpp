#pragma once
#include <string>

namespace uagfx {

enum class LogLevel { Error = 0, Info = 1, Debug = 2 };
enum class Backend { DgVoodoo, Uaro };          // [renderer] backend
enum class DisplayMode { Window, Borderless };  // [display] mode

// uaro-graphics.ini. Unknown keys and values that do not parse leave the default in place.
struct Config {
    Backend backend = Backend::DgVoodoo;
    DisplayMode mode = DisplayMode::Borderless;
    int monitor = 0;       // 0 = the main monitor
    int windowWidth = 0;   // window mode: inner size in pixels; 0 = the size the game asks for
    int windowHeight = 0;
    bool vsync = true;
    int fpsLimit = 0;      // 0 = no limit
    bool allowTearing = false;  // let G-SYNC / FreeSync monitors show frames as soon as they are ready
    LogLevel logLevel = LogLevel::Info;
};

Config parseConfig(const std::string& text);
const char* backendToken(Backend b);
const char* displayModeToken(DisplayMode m);

}  // namespace uagfx
