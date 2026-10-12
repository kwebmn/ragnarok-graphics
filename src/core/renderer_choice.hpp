#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx {

// What the running system offers; the Windows side finds out at start-up.
struct SystemFacts {
    bool windows10 = false;  // Windows 10 or newer (flip-model swap chains, D3D12)
    bool d3d12 = false;      // a Direct3D 12 device (feature level 11_0) could be created
    bool dgvoodoo = false;   // dgvoodoo\DDraw.dll exists next to the game
};

enum class Renderer { Uaro, DgVoodoo, System };

struct RendererChoice {
    Renderer renderer;
    std::string reason;  // one line for the log
};

// uaroBuilt: false while this version has no uaRO renderer yet (0.1.x passes the game through).
RendererChoice chooseRenderer(const Config& config, const SystemFacts& facts, bool uaroBuilt);
const char* rendererName(Renderer r);

}  // namespace uagfx
