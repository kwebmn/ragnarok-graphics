#pragma once
#include <string>
#include <vector>

#include "core/settings/screen.hpp"
#include "win/system_probe.hpp"

namespace uagfx::app {

// What the Screen and Diagnostics pages need from Windows (stage 2 spec §5, §7), read once at start.
struct SystemInfo {
    win::WindowsVersion windows;
    bool d3d12Probed = false;  // only on Windows 10 and newer
    win::D3D12Probe d3d12;
    win::D3D11Probe d3d11;                        // 0.46: on every Windows
    std::vector<win::AdapterProbe> adapters;      // 0.50.2: every card, the fast one first
    std::string gpuDriver;                        // "32.0.15.6094": the main adapter's driver, "" when unknown
    std::vector<settings::MonitorInfo> monitors;  // as the driver numbers them: 0 = the main one
    settings::Frame frame;                        // the game window's frame: its own or ours, the larger
    bool elevated = false;                        // this app runs as administrator
    Computer computer;                             // 2.5.15: the processor, the memory, the power (the report)
};
SystemInfo probeSystem();

// Whether uaRO.exe from this folder runs (spec §4.1); a uaRO.exe whose path cannot be read counts as running.
bool gameRunning(const std::wstring& gameExe);

}  // namespace uagfx::app
