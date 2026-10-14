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
        case Page::Smoothing: return Text::PageSmoothing;
        case Page::Ai: return Text::PageAi;
        case Page::AiCache: return Text::PageAiCache;
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
        {"renderer", "bridge", Page::Render, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderBridge,
         Text::RenderBridgeHint, ""},
        {"renderer", "remix", Page::Render, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderRemix,
         Text::RenderRemixHint, ""},
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
        {"quality", "dlss", Page::Smoothing, Control::Choice, 0, 0, 0,
         {"off", "dlaa", "quality", "balanced", "performance", "ultra"},
         {Text::SmoothOff, Text::DlssDlaa, Text::DlssQuality, Text::DlssBalanced, Text::DlssPerformance,
          Text::DlssUltra},
         Text::SmoothDlss, Text::SmoothDlssHint, ""},
        {"quality", "msaa", Page::Smoothing, Control::Choice, 0, 0, 0, {"0", "2", "4", "8"},
         {Text::SmoothOff, Text::Factor2, Text::Factor4, Text::Factor8}, Text::SmoothMsaa, Text::SmoothMsaaHint, ""},
        {"quality", "alpha_to_coverage", Page::Smoothing, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::SmoothA2c,
         Text::SmoothA2cHint, ""},
        {"quality", "anisotropy", Page::Smoothing, Control::Choice, 0, 0, 0, {"0", "2", "4", "8", "16"},
         {Text::SmoothOff, Text::Factor2, Text::Factor4, Text::Factor8, Text::Factor16}, Text::SmoothAniso,
         Text::SmoothAnisoHint, ""},
        {"ai", "enabled", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiEnable, Text::AiEnableHint, ""},
        {"ai", "ground", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiGround, Text::AiGroundHint, ""},
        {"ai", "objects", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiObjects, Text::AiObjectsHint,
         ""},
        {"ai", "monsters", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiSprites, Text::AiSpritesHint,
         ""},
        {"ai", "players", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiPlayers, Text::AiPlayersHint, ""},
        {"ai", "npcs", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiNpcs, Text::AiNpcsHint, ""},
        {"ai", "items", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiItems, Text::AiItemsHint, ""},
        {"ai", "effects", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiEffects, Text::AiEffectsHint,
         ""},
        {"ai", "icons", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiIcons, Text::AiIconsHint, ""},
        {"ai", "ui_pieces", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiUiPieces, Text::AiUiPiecesHint,
         ""},
        {"ai", "model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime"},
         {Text::AiModelGeneral, Text::AiModelAnime}, Text::AiModelWorld, Text::AiModelWorldHint, ""},
        {"ai", "sprite_model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime"},
         {Text::AiModelGeneral, Text::AiModelAnime}, Text::AiModelSprites, Text::AiModelSpritesHint, ""},
        {"ai", "denoise", Page::Ai, Control::Slider, 0, 100, 5, {}, {}, Text::AiDenoise, Text::AiDenoiseHint, " %"},
        {"ai", "scale", Page::Ai, Control::Choice, 0, 0, 0, {"4", "2"}, {Text::AiScale4, Text::AiScale2},
         Text::AiScale, Text::AiScaleHint, ""},
        {"ai", "budget", Page::Ai, Control::Number, 1, 20, 1, {}, {}, Text::AiBudget, Text::AiBudgetHint, ""},
        {"ai", "min_size", Page::Ai, Control::Number, 16, 1024, 1, {}, {}, Text::AiSizes, Text::AiSizesHint, ""},
        {"ai", "max_size", Page::Ai, Control::Number, 16, 1024, 1, {}, {}, Text::AiSizes, Text::AiSizesHint, ""},
        {"ai", "ahead", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiAhead, Text::AiAheadHint, ""},
        {"frames", "vsync", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesVsync,
         Text::FramesVsyncHint, ""},
        {"frames", "frame_generation", Page::Frames, Control::Choice, 0, 0, 0, {"off", "2x", "3x", "4x"},
         {Text::SmoothOff, Text::Factor2, Text::Times3, Text::Factor4}, Text::FramesGeneration,
         Text::FramesGenerationHint, ""},
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
    if (k == "renderer.bridge") return c.bridge ? "1" : "0";
    if (k == "renderer.remix") return c.remix ? "1" : "0";
    if (k == "display.high_resolution") return c.highResolution ? "1" : "0";
    if (k == "quality.render_scale") return std::to_string(c.renderScale);
    if (k == "quality.sharpen") return std::to_string(c.sharpen);
    if (k == "display.ui_filter") return uiFilterToken(c.uiFilter);
    if (k == "display.crisp_text") return c.crispText ? "1" : "0";
    if (k == "quality.dlss") return c.dlss;
    if (k == "quality.msaa") return std::to_string(c.msaa);
    if (k == "quality.alpha_to_coverage") return c.alphaToCoverage ? "1" : "0";
    const AiSettings& a = c.ai;
    if (k == "ai.enabled") return a.enabled ? "1" : "0";
    if (k == "ai.ground") return a.ground ? "1" : "0";
    if (k == "ai.objects") return a.objects ? "1" : "0";
    if (k == "ai.monsters") return a.monsters ? "1" : "0";
    if (k == "ai.players") return a.players ? "1" : "0";
    if (k == "ai.npcs") return a.npcs ? "1" : "0";
    if (k == "ai.items") return a.items ? "1" : "0";
    if (k == "ai.effects") return a.effects ? "1" : "0";
    if (k == "ai.icons") return a.icons ? "1" : "0";
    if (k == "ai.ui_pieces") return a.uiPieces ? "1" : "0";
    if (k == "ai.model") return a.model;
    if (k == "ai.sprite_model") return a.spriteModel;
    if (k == "ai.denoise") return std::to_string(a.denoise);
    if (k == "ai.scale") return std::to_string(a.scale);
    if (k == "ai.budget") return std::to_string(a.budget);
    if (k == "ai.min_size") return std::to_string(a.minSize);
    if (k == "ai.max_size") return std::to_string(a.maxSize);
    if (k == "ai.ahead") return a.ahead ? "1" : "0";
    if (k == "quality.anisotropy") return std::to_string(c.anisotropy);
    if (k == "frames.vsync") return c.vsync ? "1" : "0";
    if (k == "frames.fps_limit") return std::to_string(c.fpsLimit);
    if (k == "frames.frame_generation") return c.frameGeneration ? std::to_string(c.frameGeneration) + "x" : "off";
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
