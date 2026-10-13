#include "core/settings/schema.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace uagfx::settings {

Text pageName(Page p) {
    switch (p) {
        case Page::Render: return Text::PageRender;
        case Page::Screen: return Text::PageScreen;
        case Page::Quality: return Text::PageQuality;
        case Page::Frames: return Text::PageFrames;
        case Page::Sound: return Text::PageSound;
        case Page::Game: return Text::PageGame;
        case Page::Chat: return Text::PageChat;
        case Page::Diagnostics: return Text::PageDiagnostics;
        case Page::Service: break;
    }
    return Text::PageService;
}

const std::vector<IniSetting>& iniSettings() {
    static const std::vector<IniSetting> list{
        {"renderer", "backend", Page::Render, Control::Choice, 0, 0, 0, {"uaro", "dgvoodoo"},
         {Text::RenderUaro, Text::RenderDgVoodoo}, Text::RenderHeading, Text::RenderUaroHint, ""},
        {"display", "high_resolution", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityHighRes,
         Text::QualityHighResHint, ""},
        {"quality", "render_scale", Page::Quality, Control::Slider, 100, 200, 10, {}, {}, Text::QualityRenderScale,
         Text::QualityRenderScaleHint, " %"},
        {"quality", "sharpen", Page::Quality, Control::Slider, 0, 100, 5, {}, {}, Text::QualitySharpen,
         Text::QualitySharpenHint, ""},
        {"display", "ui_filter", Page::Quality, Control::Choice, 0, 0, 0, {"sharp", "smooth"},
         {Text::UiSharp, Text::UiSmooth}, Text::QualityUiFilter, Text::QualityUiFilterHint, ""},
        {"display", "crisp_text", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityCrispText,
         Text::QualityCrispTextHint, ""},
        {"frames", "vsync", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesVsync,
         Text::FramesVsyncHint, ""},
        {"frames", "fps_limit", Page::Frames, Control::Number, 0, 1000, 1, {}, {}, Text::FramesFpsLimit,
         Text::FramesFpsLimitHint, ""},
        {"frames", "allow_tearing", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesTearing,
         Text::FramesTearingHint, ""},
        {"debug", "log", Page::Diagnostics, Control::Check, 0, 1, 1, {"normal", "verbose"}, {}, Text::DiagVerboseLog,
         Text::DiagVerboseLogHint, ""},
    };
    return list;
}

const IniSetting* findSetting(const std::string& sectionKey) {
    for (const IniSetting& s : iniSettings())
        if (sectionKey == std::string(s.section) + "." + s.key) return &s;
    return nullptr;
}

std::string sectionKey(const IniSetting& s) { return std::string(s.section) + "." + s.key; }

std::string tokenOf(const IniSetting& s, const Config& c) {
    const std::string k = sectionKey(s);
    if (k == "renderer.backend") return backendToken(c.backend);
    if (k == "display.high_resolution") return c.highResolution ? "1" : "0";
    if (k == "quality.render_scale") return std::to_string(c.renderScale);
    if (k == "quality.sharpen") return std::to_string(c.sharpen);
    if (k == "display.ui_filter") return uiFilterToken(c.uiFilter);
    if (k == "display.crisp_text") return c.crispText ? "1" : "0";
    if (k == "frames.vsync") return c.vsync ? "1" : "0";
    if (k == "frames.fps_limit") return std::to_string(c.fpsLimit);
    if (k == "frames.allow_tearing") return c.allowTearing ? "1" : "0";
    if (k == "debug.log") return c.logLevel == LogLevel::Debug ? "verbose" : "normal";
    return "";
}

bool validToken(const IniSetting& s, const std::string& token) {
    if (s.control == Control::Slider || s.control == Control::Number) {
        char* end = nullptr;
        const long n = std::strtol(token.c_str(), &end, 10);
        return !token.empty() && *end == '\0' && n >= s.min && n <= s.max;
    }
    return std::any_of(s.tokens.begin(), s.tokens.end(), [&](const char* t) { return token == t; });
}

}  // namespace uagfx::settings
