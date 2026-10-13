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

}  // namespace uagfx::win
