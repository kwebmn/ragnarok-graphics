#include "core/renderer_choice.hpp"

namespace uagfx {

RendererChoice chooseRenderer(const Config& config, const SystemFacts& facts, bool uaroBuilt) {
    std::string why;
    if (config.backend == Backend::Uaro) {
        if (!uaroBuilt) {
            why = "backend = uaro, but the uaRO renderer is not in this version";
        } else if (!facts.windows10) {
            why = "backend = uaro, but the uaRO renderer needs Windows 10 or newer";
        } else if (!facts.d3d12) {
            why = "backend = uaro, but no Direct3D 12 device could be created";
        } else {
            return {Renderer::Uaro, "backend = uaro"};
        }
    } else {
        why = "backend = dgvoodoo";
    }
    if (facts.dgvoodoo) return {Renderer::DgVoodoo, why};
    return {Renderer::System, why + "; dgvoodoo\\DDraw.dll not found"};
}

const char* rendererName(Renderer r) {
    switch (r) {
        case Renderer::Uaro: return "uaRO Graphics";
        case Renderer::DgVoodoo: return "dgVoodoo";
        case Renderer::System: break;
    }
    return "system DirectDraw";
}

}  // namespace uagfx
