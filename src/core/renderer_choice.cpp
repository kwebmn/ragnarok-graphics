#include "core/renderer_choice.hpp"

namespace uagfx {

RendererChoice chooseRenderer(const Config& config, const SystemFacts& facts, bool uaroBuilt) {
    std::string why;
    if (config.backend == Backend::Uaro && config.api != Api::D3d12 && uaroBuilt) {
        // 0.46: Direct3D 11 - asked for, or (auto) where Direct3D 12 is not there; no Windows 10 needed.
        const bool d3d12 = facts.windows10 && facts.d3d12;
        if (config.api == Api::Auto && d3d12) return {Renderer::Uaro, "backend = uaro, api = auto: Direct3D 12"};
        const std::string asked = config.api == Api::Auto
                                      ? std::string("backend = uaro, api = auto, no Direct3D 12")
                                      : std::string("backend = uaro, api = d3d11");
        if (facts.d3d11) return {Renderer::Uaro, asked + ": Direct3D 11", Api::D3d11};
        why = asked + ", but no Direct3D 11 device could be created";
    } else if (config.backend == Backend::Uaro) {
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
