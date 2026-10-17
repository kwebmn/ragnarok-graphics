#include "core/settings/screen.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::settings {
namespace {

int scaled(int v, int numerator, int denominator) {
    return int(std::lround(double(v) * numerator / denominator));
}

Size scaled(Size s, int numerator, int denominator) {
    return Size{scaled(s.width, numerator, denominator), scaled(s.height, numerator, denominator)};
}

double fit(Size out, Size game) {
    if (game.width <= 0 || game.height <= 0) return 1.0;
    return std::min(double(out.width) / game.width, double(out.height) / game.height);
}

// The game's own window (the picture and its frame, logical pixels) inside the main monitor's work area.
bool gameWindowFits(Size game, const MonitorInfo& primary, const Frame& frame) {
    const Size work = scaled(primary.work, 100, primary.scale);
    return game.width + frame.width <= work.width && game.height + frame.height <= work.height;
}

// A window of W real pixels with its frame (at the monitor's scale) inside the monitor's work area.
bool windowFits(Size w, const MonitorInfo& m, const Frame& frame) {
    return w.width + scaled(frame.width, m.scale, 100) <= m.work.width &&
           w.height + scaled(frame.height, m.scale, 100) <= m.work.height;
}

}  // namespace

ScreenPlan planScreen(const ScreenChoice& c, const std::vector<MonitorInfo>& monitors, const RoextWindow& roext,
                      const Frame& frame) {
    ScreenPlan p;
    if (monitors.empty()) {
        p.problem = ScreenProblem::BadNumber;
        return p;
    }
    const MonitorInfo& primary = monitors[0];
    const int index = c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? c.monitor : 0;
    const MonitorInfo& m = monitors[size_t(index)];
    if (c.show == Show::Fullscreen) {
        p.fullscreen = true;
        if (c.ui < kUiMin || c.ui > kUiMax) {
            p.problem = ScreenProblem::BadNumber;
            return p;
        }
        p.game = scaled(m.pixels, 100, c.ui);
        p.writesIni = true;
        p.mode = DisplayMode::Auto;
        p.monitor = index;
        p.s = fit(m.pixels, p.game);
    } else {
        if (c.window.width <= 0 || c.window.height <= 0) {
            p.problem = ScreenProblem::BadNumber;
            return p;
        }
        const int ui = c.ui;
        if (ui < kUiMin || ui > kUiMax) {
            p.problem = ScreenProblem::BadNumber;
            return p;
        }
        p.game = gameSize(c.window, ui);
        p.writesIni = true;
        p.monitor = index;
        if (ui == m.scale && m.primary) {
            p.mode = DisplayMode::Auto;  // the game's own window, as ROExt or uaRO Pad make it
        } else {
            p.mode = DisplayMode::Window;
            p.iniWindow = scaled(c.window, 100, m.scale);
        }
        p.writesRoext = roext.active && (roext.lock || roext.size.width > 0);
        p.s = fit(c.window, p.game);
        if (!windowFits(c.window, m, frame) || !gameWindowFits(p.game, primary, frame)) p.problem = ScreenProblem::TooBig;
    }
    if (p.problem == ScreenProblem::None && (p.game.width < kGameMin.width || p.game.height < kGameMin.height))
        p.problem = ScreenProblem::TooSmall;
    return p;
}

const std::vector<int>& uiSteps() {
    static const std::vector<int> steps{100, 125, 150, 175, 200, 250, 300};
    return steps;
}

const Size kStandard[] = {{1920, 1080}, {1600, 900}, {1366, 768}, {1280, 720}, {1024, 768}};

std::vector<Size> windowSizes(const MonitorInfo& m, const Frame& frame) {
    std::vector<Size> out;
    auto add = [&](Size w) {
        if (w.width > 0 && w.height > 0 && windowFits(w, m, frame) && std::find(out.begin(), out.end(), w) == out.end())
            out.push_back(w);
    };
    for (Size game : kStandard) add(scaled(game, m.scale, 100));
    if (m.pixels.width > 0 && m.pixels.height > 0) {  // the largest with the monitor's proportions
        Size w;
        w.height = m.work.height - scaled(frame.height, m.scale, 100);
        w.width = scaled(w.height, m.pixels.width, m.pixels.height);
        const int roomX = m.work.width - scaled(frame.width, m.scale, 100);
        if (w.width > roomX) {
            w.width = roomX;
            w.height = scaled(w.width, m.pixels.height, m.pixels.width);
        }
        add(w);
    }
    std::sort(out.begin(), out.end(),
              [](Size a, Size b) { return long(a.width) * a.height > long(b.width) * b.height; });
    return out;
}

Size defaultWindow(const MonitorInfo& m, const Frame& frame) {
    for (Size game : kStandard) {
        const Size w = scaled(game, m.scale, 100);
        if (windowFits(w, m, frame)) return w;
    }
    return Size{};
}

ScreenNow currentScreen(bool regFullscreen, Size regSize, const Config& ini, const RoextWindow& roext,
                        const std::vector<MonitorInfo>& monitors) {
    ScreenNow n;
    if (monitors.empty()) return n;
    const int index = ini.monitor >= 0 && size_t(ini.monitor) < monitors.size() ? ini.monitor : 0;
    const MonitorInfo& m = monitors[size_t(index)];
    const MonitorInfo& primary = monitors[0];
    n.game = regSize;
    const bool borderless = ini.mode == DisplayMode::Borderless;  // a hand-written ini: over the monitor too
    if (regFullscreen || borderless) {
        n.choice.show = Show::Fullscreen;
        n.choice.monitor = index;
        n.choice.ui = regSize.width > 0 ? scaled(m.pixels.width, 100, regSize.width) : 100;
        return n;
    }
    n.choice.show = Show::Window;
    if (roext.active && roext.size.width > 0 && roext.size.height > 0) {
        n.game = roext.size;
        n.roextOverride = true;
    } else if (roext.active && roext.lock) {
        n.game = scaled(primary.work, 100, primary.scale);
        n.roextOverride = true;
    }
    if (ini.mode == DisplayMode::Window) {
        n.choice.monitor = index;
        const Size logical{ini.windowWidth > 0 ? ini.windowWidth : n.game.width,
                           ini.windowHeight > 0 ? ini.windowHeight : n.game.height};
        n.choice.window = scaled(logical, m.scale, 100);
    } else {
        n.choice.monitor = 0;
        n.choice.window = scaled(n.game, primary.scale, 100);
    }
    n.choice.ui = n.game.width > 0 ? scaled(n.choice.window.width, 100, n.game.width) : 100;
    return n;
}

bool integerScale(double s) { return s >= 1.999 && std::fabs(s - std::round(s)) < 0.001; }

Size gameSize(Size window, int ui) { return scaled(window, 100, ui); }

}  // namespace uagfx::settings
