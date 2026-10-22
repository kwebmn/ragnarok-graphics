#include "core/renderer_choice.hpp"

namespace uagfx {

RendererChoice chooseRenderer(const Config& config, const SystemFacts& facts, bool uaroBuilt) {
    // 2.2: the uaRO renderer always (dgVoodoo is gone); where it cannot start, the system's DirectDraw.
    std::string why;
    if (!uaroBuilt) {
        why = "the uaRO renderer is not in this version";
    } else if (config.api == Api::System) {
        why = "api = system: uaRO Graphics switched off - the game's own look";  // 2.12
    } else if (config.api != Api::D3d12) {
        // 0.46: Direct3D 11 - asked for, or (auto) where Direct3D 12 is not there; no Windows 10 needed.
        const bool d3d12 = facts.windows10 && facts.d3d12;
        if (config.api == Api::Auto && d3d12) return {Renderer::Uaro, "api = auto: Direct3D 12"};
        const std::string asked =
            config.api == Api::Auto ? std::string("api = auto, no Direct3D 12") : std::string("api = d3d11");
        if (facts.d3d11) return {Renderer::Uaro, asked + ": Direct3D 11", Api::D3d11};
        why = asked + ", but no Direct3D 11 device could be created";
    } else if (!facts.windows10) {
        why = "the uaRO renderer needs Windows 10 or newer";
    } else if (!facts.d3d12) {
        why = "no Direct3D 12 device could be created";
    } else {
        return {Renderer::Uaro, "api = d3d12"};
    }
    return {Renderer::System, why};
}

const char* rendererName(Renderer r) { return r == Renderer::Uaro ? "uaRO Graphics" : "system DirectDraw"; }

}  // namespace uagfx
