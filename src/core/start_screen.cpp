#include "core/start_screen.hpp"

#include <algorithm>

namespace uagfx::start {

std::string line(const Progress& p) {
    switch (p.stage) {
        case Stage::Renderer: return "Starting the renderer...";
        case Stage::Dlss: return "Starting NVIDIA DLSS...";
        case Stage::ShadersLoad: return "Loading shaders...";
        case Stage::ShadersBuild:
            return "Compiling shaders: " + std::to_string(std::max(p.done, 0)) + " of " + std::to_string(p.total);
        case Stage::AiTextures: return "Starting AI textures...";
    }
    return "";
}

std::string hint(const Progress& p) {
    return p.stage == Stage::ShadersBuild ? "First start of this version - the next ones skip this" : "";
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
