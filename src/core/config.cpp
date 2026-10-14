#include "core/config.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace uagfx {
namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string lower(std::string s) {
    for (char& ch : s) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool toInt(const std::string& v, int lo, int hi, int& out) {
    char* end = nullptr;
    const long n = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || *end != '\0' || n < lo || n > hi) return false;
    out = int(n);
    return true;
}

void setBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "on") out = true;
    if (v == "0" || v == "false" || v == "off") out = false;
}

// One of a few factors ("0", "2", "4", ...); 1 means off, as 0.
void setFactor(const std::string& v, int most, int& out) {
    int n = 0;
    if (!toInt(v, 0, most, n) || (n & (n - 1)) != 0) return;  // 0 or a power of two
    out = n == 1 ? 0 : n;
}

// A window side: 0 (the game's own size) or a real size.
void setSide(const std::string& v, int& out) {
    int n = 0;
    if (toInt(v, 0, 16384, n) && (n == 0 || n >= 320)) out = n;
}

}  // namespace

Config parseConfig(const std::string& text) {
    Config c;
    std::istringstream in(text);
    std::string line;
    std::string section;
    bool first = true;
    while (std::getline(in, line)) {
        if (first && line.rfind("\xEF\xBB\xBF", 0) == 0) line = line.substr(3);  // saved by Notepad with a BOM
        first = false;
        const size_t comment = line.find(';');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = section + "." + lower(trim(line.substr(0, eq)));
        const std::string val = lower(trim(line.substr(eq + 1)));
        if (key == "renderer.backend") {
            if (val == "dgvoodoo") c.backend = Backend::DgVoodoo;
            if (val == "uaro") c.backend = Backend::Uaro;
        } else if (key == "renderer.bridge") {
            setBool(val, c.bridge);
        } else if (key == "renderer.bridge_present") {
            if (val == "auto" || val == "window" || val == "composition") c.bridgePresent = val;
        } else if (key == "renderer.remix") {
            setBool(val, c.remix);
        } else if (key == "renderer.bridge_dpi_scale") {
            setBool(val, c.bridgeDpiScale);
        } else if (key == "display.mode") {
            if (val == "auto") c.mode = DisplayMode::Auto;
            if (val == "window") c.mode = DisplayMode::Window;
            if (val == "borderless") c.mode = DisplayMode::Borderless;
        } else if (key == "display.high_resolution") {
            setBool(val, c.highResolution);
        } else if (key == "display.ui_filter") {
            if (val == "sharp") c.uiFilter = UiFilter::Sharp;
            if (val == "smooth") c.uiFilter = UiFilter::Smooth;
        } else if (key == "display.crisp_text") {
            setBool(val, c.crispText);
        } else if (key == "quality.render_scale") {
            toInt(val, 100, 200, c.renderScale);
        } else if (key == "quality.sharpen") {
            toInt(val, 0, 100, c.sharpen);
        } else if (key == "quality.msaa") {
            setFactor(val, 8, c.msaa);
        } else if (key == "quality.alpha_to_coverage") {
            setBool(val, c.alphaToCoverage);
        } else if (key == "quality.dlss") {
            if (val == "off" || val == "dlaa" || val == "quality" || val == "balanced" || val == "performance" ||
                val == "ultra")
                c.dlss = val;
        } else if (key == "quality.dlss_jitter") {
            if (val == "normal" || val == "inverted") c.dlssJitterInverted = val == "inverted";
        } else if (key == "quality.dlss_motion") {
            if (val == "normal" || val == "inverted") c.dlssMotionInverted = val == "inverted";
        } else if (key == "quality.anisotropy") {
            setFactor(val, 16, c.anisotropy);
        } else if (key == "quality.ai_textures") {
            setBool(val, c.aiTextures);
        } else if (key == "quality.ai_budget") {
            toInt(val, 1, 20, c.aiBudget);
        } else if (key == "display.monitor") {
            toInt(val, 0, 16, c.monitor);
        } else if (key == "display.window_width") {
            setSide(val, c.windowWidth);
        } else if (key == "display.window_height") {
            setSide(val, c.windowHeight);
        } else if (key == "frames.vsync") {
            setBool(val, c.vsync);
        } else if (key == "frames.frame_generation") {
            if (val == "off") c.frameGeneration = 0;
            if (val == "2x" || val == "3x" || val == "4x") c.frameGeneration = val[0] - '0';
        } else if (key == "frames.fps_limit") {
            toInt(val, 0, 1000, c.fpsLimit);
        } else if (key == "frames.allow_tearing") {
            setBool(val, c.allowTearing);
        } else if (key == "debug.log") {
            if (val == "normal") c.logLevel = LogLevel::Info;
            if (val == "verbose") c.logLevel = LogLevel::Debug;
        }
    }
    return c;
}

const char* backendToken(Backend b) { return b == Backend::Uaro ? "uaro" : "dgvoodoo"; }

const char* displayModeToken(DisplayMode m) {
    switch (m) {
        case DisplayMode::Window: return "window";
        case DisplayMode::Borderless: return "borderless";
        case DisplayMode::Auto: break;
    }
    return "auto";
}

const char* uiFilterToken(UiFilter f) { return f == UiFilter::Smooth ? "smooth" : "sharp"; }

}  // namespace uagfx
