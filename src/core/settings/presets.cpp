#include "core/settings/presets.hpp"

namespace uagfx::settings {
namespace {

struct Values {
    bool highResolution;
    int renderScale;
    int sharpen;
    bool crispText;
};

Values valuesOf(Preset p) {
    switch (p) {
        case Preset::Maximum: return {true, 200, 50, true};
        case Preset::Balance: return {true, 100, 50, true};
        case Preset::Speed: return {true, 100, 0, false};
        case Preset::Plain: break;
    }
    return {false, 100, 0, false};
}

}  // namespace

std::vector<IniEdit> presetEdits(Preset p) {
    const Values v = valuesOf(p);
    return {{"display", "high_resolution", v.highResolution ? "1" : "0"},
            {"quality", "render_scale", std::to_string(v.renderScale)},
            {"quality", "sharpen", std::to_string(v.sharpen)},
            {"display", "ui_filter", "sharp"},
            {"display", "crisp_text", v.crispText ? "1" : "0"}};
}

std::optional<Preset> presetOf(const Config& c) {
    for (int i = 0; i < kPresetCount; ++i) {
        const Values v = valuesOf(Preset(i));
        if (c.highResolution == v.highResolution && c.renderScale == v.renderScale && c.sharpen == v.sharpen &&
            c.uiFilter == UiFilter::Sharp && c.crispText == v.crispText)
            return Preset(i);
    }
    return std::nullopt;
}

}  // namespace uagfx::settings
