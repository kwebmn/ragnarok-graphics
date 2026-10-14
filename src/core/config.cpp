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

bool isBool(const std::string& v) {
    return v == "1" || v == "true" || v == "on" || v == "0" || v == "false" || v == "off";
}

// One key of [ai]; enabled, budget and denoise say whether the file had a valid one (0.11's keys yield to it).
void parseAi(const std::string& k, const std::string& v, AiSettings& a, bool& enabled, bool& budget, bool& denoise) {
    if (k == "enabled") {
        setBool(v, a.enabled);
        enabled = enabled || isBool(v);
    } else if (k == "ground") {
        setBool(v, a.ground);
    } else if (k == "objects") {
        setBool(v, a.objects);
    } else if (k == "sprites") {
        setBool(v, a.sprites);
    } else if (k == "effects") {
        setBool(v, a.effects);
    } else if (k == "model" || k == "sprite_model") {
        if (v == "general" || v == "anime") (k == "model" ? a.model : a.spriteModel) = v;
    } else if (k == "denoise") {
        denoise = toInt(v, 0, 100, a.denoise) || denoise;
    } else if (k == "scale") {
        if (v == "4" || v == "2") a.scale = v[0] - '0';
    } else if (k == "budget") {
        budget = toInt(v, 1, 20, a.budget) || budget;
    } else if (k == "min_size") {
        toInt(v, 16, 1024, a.minSize);
    } else if (k == "max_size") {
        toInt(v, 16, 1024, a.maxSize);
    } else if (k == "ahead") {
        setBool(v, a.ahead);
    }
}

}  // namespace

bool AiSettings::operator==(const AiSettings& o) const {
    return enabled == o.enabled && ground == o.ground && objects == o.objects && sprites == o.sprites &&
           effects == o.effects && model == o.model && spriteModel == o.spriteModel && denoise == o.denoise &&
           scale == o.scale && budget == o.budget && minSize == o.minSize && maxSize == o.maxSize && ahead == o.ahead;
}

AiPreset aiPresetOf(const AiSettings& a) {
    for (AiPreset p : {AiPreset::Recommended, AiPreset::All})
        if (aiPresetSettings(p, a.enabled) == a) return p;
    return AiPreset::Custom;
}

AiSettings aiPresetSettings(AiPreset p, bool enabled) {
    AiSettings a;
    a.enabled = enabled;
    if (p == AiPreset::All) a.sprites = a.effects = true;
    return a;
}

bool aiKind(const AiSettings& a, int kind) {
    switch (kind) {
        case 0: return a.ground;
        case 1: return a.objects;
        case 2: return a.sprites;
        case 3: return a.effects;
        default: return false;
    }
}

std::string aiArgument(const AiSettings& a) {
    const auto b = [](bool v) { return v ? "1" : "0"; };
    return std::string("enabled=") + b(a.enabled) + ";ground=" + b(a.ground) + ";objects=" + b(a.objects) +
           ";sprites=" + b(a.sprites) + ";effects=" + b(a.effects) + ";model=" + a.model +
           ";sprite_model=" + a.spriteModel + ";denoise=" + std::to_string(a.denoise) +
           ";scale=" + std::to_string(a.scale) + ";budget=" + std::to_string(a.budget) +
           ";min_size=" + std::to_string(a.minSize) + ";max_size=" + std::to_string(a.maxSize) + ";ahead=" + b(a.ahead);
}

AiSettings parseAiArgument(const std::string& word) {
    std::string text = "[ai]\n";
    for (char ch : word) text += ch == ';' ? '\n' : ch;
    return parseConfig(text).ai;
}

Config parseConfig(const std::string& text) {
    Config c;
    // 0.11's keys count only where [ai] has no valid key of its own, wherever they stand in the file.
    AiSettings legacy;
    bool oldEnabled = false, oldBudget = false, oldDenoise = false;
    bool newEnabled = false, newBudget = false, newDenoise = false;
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
            setBool(val, legacy.enabled);
            oldEnabled = oldEnabled || isBool(val);
        } else if (key == "quality.ai_budget") {
            oldBudget = toInt(val, 1, 20, legacy.budget) || oldBudget;
        } else if (key == "quality.ai_denoise") {
            oldDenoise = toInt(val, 0, 100, legacy.denoise) || oldDenoise;
        } else if (key.compare(0, 3, "ai.") == 0) {
            parseAi(key.substr(3), val, c.ai, newEnabled, newBudget, newDenoise);
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
    if (!newEnabled && oldEnabled) c.ai.enabled = legacy.enabled;
    if (!newBudget && oldBudget) c.ai.budget = legacy.budget;
    if (!newDenoise && oldDenoise) c.ai.denoise = legacy.denoise;
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
