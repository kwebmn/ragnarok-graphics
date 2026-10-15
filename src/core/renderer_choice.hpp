#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx {

// What the running system offers; the Windows side finds out at start-up.
struct SystemFacts {
    bool windows10 = false;  // Windows 10 or newer (flip-model swap chains, D3D12)
    bool d3d12 = false;      // a Direct3D 12 device (feature level 11_0) could be created
    bool dgvoodoo = false;   // dgvoodoo\DDraw.dll exists next to the game
    bool d3d11 = false;      // 0.46: a Direct3D 11 device (feature level 10_0 or better) could be created
};

enum class Renderer { Uaro, DgVoodoo, System };

struct RendererChoice {
    Renderer renderer;
    std::string reason;    // one line for the log
    Api api = Api::D3d12;  // 0.46: the uaRO renderer's: Direct3D 12 or 11 (never Auto)
};

// uaroBuilt: false while this version has no uaRO renderer yet (0.1.x passes the game through).
RendererChoice chooseRenderer(const Config& config, const SystemFacts& facts, bool uaroBuilt);
const char* rendererName(Renderer r);

}  // namespace uagfx
