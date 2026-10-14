#include "core/settings/presets.hpp"

namespace uagfx::settings {
namespace {

struct Values {
    bool highResolution;
    int renderScale;
    int sharpen;
    bool crispText;
    int msaa;  // stage 4
    bool alphaToCoverage;
    int anisotropy;
};

Values valuesOf(Preset p) {
    switch (p) {
        case Preset::Maximum: return {true, 200, 50, true, 4, true, 16};
        case Preset::Balance: return {true, 100, 50, true, 4, true, 16};
        case Preset::Speed: return {true, 100, 0, false, 0, true, 4};
        case Preset::Plain: break;
    }
    return {false, 100, 0, false, 0, false, 0};
}

}  // namespace

std::vector<IniEdit> presetEdits(Preset p) {
    const Values v = valuesOf(p);
    return {{"display", "high_resolution", v.highResolution ? "1" : "0"},
            {"quality", "render_scale", std::to_string(v.renderScale)},
            {"quality", "sharpen", std::to_string(v.sharpen)},
            {"display", "ui_filter", "sharp"},
            {"display", "crisp_text", v.crispText ? "1" : "0"},
            {"quality", "msaa", std::to_string(v.msaa)},
            {"quality", "alpha_to_coverage", v.alphaToCoverage ? "1" : "0"},
            {"quality", "anisotropy", std::to_string(v.anisotropy)}};
}

std::vector<std::pair<std::string, std::string>> presetRegWords(Preset p) {
    switch (p) {
        case Preset::Maximum:
        case Preset::Balance: return {{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}};
        case Preset::Speed: return {{"TEXTUREMODE", "1"}};
        case Preset::Plain: break;
    }
    return {};
}

std::optional<Preset> presetOf(const Config& c, const std::map<std::string, std::string>& gameWords) {
    for (int i = 0; i < kPresetCount; ++i) {
        const Values v = valuesOf(Preset(i));
        bool same = c.highResolution == v.highResolution && c.renderScale == v.renderScale && c.sharpen == v.sharpen &&
                    c.uiFilter == UiFilter::Sharp && c.crispText == v.crispText && c.msaa == v.msaa &&
                    c.alphaToCoverage == v.alphaToCoverage && c.anisotropy == v.anisotropy;
        for (const auto& w : presetRegWords(Preset(i))) {
            const auto it = gameWords.find(w.first);
            same = same && (it == gameWords.end() || it->second == w.second);
        }
        if (same) return Preset(i);
    }
    return std::nullopt;
}

}  // namespace uagfx::settings
