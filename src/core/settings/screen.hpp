#pragma once
#include <vector>

#include "core/config.hpp"

namespace uagfx::settings {

struct Size {
    int width = 0;
    int height = 0;
    bool operator==(const Size& o) const { return width == o.width && height == o.height; }
    bool operator!=(const Size& o) const { return !(*this == o); }
};

// A monitor, at the index the driver gives it (stage 2 spec §5.2: 0 = the main one, then EnumDisplayMonitors order).
struct MonitorInfo {
    bool primary = false;
    Size pixels;              // P: its mode, real pixels
    Size work;                // A: without the task bar, real pixels
    int scale = 100;          // d: Windows scaling, %
    int refresh = 0;          // Hz
    bool hdr = false;
    std::vector<Size> modes;  // its real 32-bit modes, largest first (dgVoodoo's full screen)
};

enum class Show { Fullscreen, Window };

// What the player picks on the Screen page (spec §5.2).
struct ScreenChoice {
    Show show = Show::Window;
    int monitor = 0;  // index as the driver numbers them
    int ui = 100;     // u, %
    Size window;      // W, real pixels (in a window)
    Size mode;        // dgVoodoo's full screen: the monitor mode picked, which is L
    bool operator==(const ScreenChoice& o) const {
        return show == o.show && monitor == o.monitor && ui == o.ui && window == o.window && mode == o.mode;
    }
};

// The ROExt settings that matter for the screen (spec §4.4).
struct RoextWindow {
    bool active = false;  // an active ROExt reads this ini
    bool lock = false;    // WindowLock: no border; without its own size it covers the screen but the task bar
    Size size;            // WindowWidth x WindowHeight; 0x0 = not set
};

// What the game's own window adds around its picture (caption, border), in the game's (logical) pixels.
struct Frame {
    int width = 16;
    int height = 39;
};

constexpr int kUiMin = 100;
constexpr int kUiMax = 400;
constexpr Size kGameMin{640, 480};  // the interface needs at least this (registry.md §2)

enum class ScreenProblem { None, BadNumber, TooSmall, TooBig };

// What a choice writes (spec §5.3).
struct ScreenPlan {
    ScreenProblem problem = ScreenProblem::None;
    Size game;                // L: WIDTH x HEIGHT
    bool fullscreen = false;  // ISFULLSCREENMODE; BITPERPIXEL is always 32
    bool writesIni = false;   // uaRO Graphics: [display] mode, monitor, window_width, window_height
    DisplayMode mode = DisplayMode::Auto;
    int monitor = 0;
    Size iniWindow;           // window_width x window_height; 0x0 = the game's own size
    bool writesRoext = false; // ROExt WindowWidth x WindowHeight = game
    double s = 1.0;           // how many times the game is enlarged on screen
};
ScreenPlan planScreen(const ScreenChoice& c, const std::vector<MonitorInfo>& monitors, Backend backend,
                      const RoextWindow& roext, const Frame& frame);

const std::vector<int>& uiSteps();  // 100, 125, 150, 175, 200, 250, 300 (spec §5.2)
// The window sizes offered on a monitor (spec §5.2), largest first: the standard game sizes times d and the largest
// one with the monitor's proportions - those that fit its work area with the frame.
std::vector<Size> windowSizes(const MonitorInfo& m, const Frame& frame);
// The window of a first run without setup.exe (spec §4.3): the largest standard game size times d that fits; 0x0
// when none does.
Size defaultWindow(const MonitorInfo& m, const Frame& frame);

// What the game gets now from its stores (spec §5.3 «Сейчас»), as the choice the Screen page starts from.
struct ScreenNow {
    ScreenChoice choice;
    Size game;                   // L the game will use
    bool roextOverride = false;  // ROExt sets the windowed size (its own, or the work area with WindowLock)
};
ScreenNow currentScreen(bool regFullscreen, Size regSize, const Config& ini, const RoextWindow& roext,
                        const std::vector<MonitorInfo>& monitors, Backend backend);

bool integerScale(double s);  // ×2, ×3...: the crispest interface
// The game's size L in a window of W real pixels with the interface at u %: W·100/u, rounded (spec §5.3).
Size gameSize(Size window, int ui);

}  // namespace uagfx::settings
