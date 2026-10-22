#include "core/start_screen.hpp"

#include <algorithm>

namespace uagfx::start {

std::string line(const Progress& p, settings::Lang l) {
    using settings::Text;
    switch (p.stage) {
        case Stage::Renderer: return settings::tr(l, Text::StartRenderer);
        case Stage::Dlss: return settings::tr(l, Text::StartDlss);
        case Stage::ShadersLoad: return settings::tr(l, Text::StartShadersLoad);
        case Stage::ShadersBuild:
            return settings::fmt(settings::tr(l, Text::StartShadersBuild),
                                 {std::to_string(std::max(p.done, 0)), std::to_string(p.total)});
        case Stage::AiTextures: return settings::tr(l, Text::StartAiTextures);
    }
    return "";
}

std::string hint(const Progress& p, settings::Lang l) {
    // 2.13: while nothing else is said under the line - «Звернення»'s key
    return settings::tr(l, p.stage == Stage::ShadersBuild ? settings::Text::StartFirstHint
                                                         : settings::Text::StartReportHint);
}

float filled(const Progress& p) {
    switch (p.stage) {
        case Stage::Renderer: return 0.03f;
        case Stage::Dlss: return 0.06f;
        case Stage::ShadersLoad:
        case Stage::ShadersBuild: {
            const float share = p.total > 0 ? std::clamp(float(p.done) / float(p.total), 0.0f, 1.0f) : 0.0f;
            return 0.08f + 0.82f * share;
        }
        case Stage::AiTextures: return 0.94f;
    }
    return 0.0f;
}

}  // namespace uagfx::start
