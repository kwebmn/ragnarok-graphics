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
    std::vector<IniEdit> e = {{"display", "high_resolution", v.highResolution ? "1" : "0"},
                              {"quality", "render_scale", std::to_string(v.renderScale)},
                              {"quality", "sharpen", std::to_string(v.sharpen)},
                              {"display", "ui_filter", "sharp"},
                              {"display", "crisp_text", v.crispText ? "1" : "0"},
                              {"quality", "msaa", std::to_string(v.msaa)},
                              {"quality", "alpha_to_coverage", v.alphaToCoverage ? "1" : "0"},
                              {"quality", "anisotropy", std::to_string(v.anisotropy)},
                              // 0.85: every preset keeps the card from working for frames no one sees
                              {"frames", "fps_limit", "60"},
                              {"frames", "save_power", "1"}};
    // 0.85: and the light one leaves out the costly effects; 0.90.3: the characters' shadows too (5-10 ms of a
    // strong card's in a crowd with Direct3D 11) - the auto setup's for old and small cards
    if (p == Preset::Speed)
        for (const char* k : {"ao", "glow", "heat", "floor_reflections", "shafts",
                              "bloom", "grass", "actor_shadows"})
            e.push_back({"effects", k, "0"});
    return e;
}

std::vector<std::pair<std::string, std::string>> presetRegWords(Preset p) {
    // 0.87.1: Speed too keeps the game's textures and sprites at full size - it lowered TEXTUREMODE to 1 (the game's
    // textures at half size), and the first game after it on a Radeon HD 6570 (Direct3D 11) crashed in the game's own
    // sprite writer within seconds; the textures cost a card little since they are kept compressed (0.57).
    switch (p) {
        case Preset::Maximum:
        case Preset::Balance:
        case Preset::Speed: return {{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}};
        case Preset::Plain: break;
    }
    return {};
}

std::vector<IniEdit> mainQualityEdits(MainQuality q) {
    if (q == MainQuality::SuperLight) return {{"quality", "super_light", "1"}};
    const Preset p = q == MainQuality::Low      ? Preset::Speed
                     : q == MainQuality::Medium ? Preset::Balance
                                                : Preset::Maximum;
    std::vector<IniEdit> edits{{"quality", "super_light", "0"}};
    for (const IniEdit& e : presetEdits(p))
        if (e.section != "frames" && e.section != "effects") edits.push_back(e);
    return edits;
}

std::optional<MainQuality> mainQualityOf(const Config& c) {
    if (c.superLight) return MainQuality::SuperLight;
    switch (presetOf(c).value_or(Preset::Plain)) {
        case Preset::Speed: return MainQuality::Low;
        case Preset::Balance: return MainQuality::Medium;
        case Preset::Maximum: return MainQuality::High;
        case Preset::Plain: break;
    }
    return std::nullopt;
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
