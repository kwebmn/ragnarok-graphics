#pragma once
#include <string>

namespace uagfx::win {

struct WindowsVersion {
    unsigned long major = 0;
    unsigned long minor = 0;
    unsigned long build = 0;
};
// The real version: GetVersionEx reports Windows 8 to programs without a manifest.
WindowsVersion windowsVersion();

struct D3D12Probe {
    bool ok = false;  // a feature level 11_0 device was created on the main adapter (and released)
    long hr = 0;      // HRESULT of the failing step
    std::string adapter;
    unsigned vendor = 0;
    unsigned device = 0;
    unsigned long long vramMb = 0;
};
// d3d12.dll and dxgi.dll are loaded at run time and stay loaded for the renderer: as the game finds them, or with
// systemOnly Windows' own copies from System32 - the settings app has no business with a dxgi.dll the game folder may
// hold (ReShade), 0.6.0 review M8.
D3D12Probe probeD3D12(bool systemOnly = false);

// 0.46: a Direct3D 11 device of feature level 10_0 or better on the main adapter (made and released).
struct D3D11Probe {
    bool ok = false;
    long hr = 0;
    unsigned level = 0;  // D3D_FEATURE_LEVEL: 0xb000 is 11_0
    std::string adapter;
    unsigned vendor = 0;                // 0.50: 0x1414 - Microsoft's software adapter (no graphics driver)
    unsigned long long vramMb = 0;      // 0.50: what an old card without Direct3D 12 has (the automatic setup)
};
D3D11Probe probeD3D11(bool systemOnly = false);
std::string featureLevelName(unsigned level);  // "11_0"

}  // namespace uagfx::win
