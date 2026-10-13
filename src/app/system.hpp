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
    std::string gpuDriver;                        // "32.0.15.6094": the main adapter's driver, "" when unknown
    std::vector<settings::MonitorInfo> monitors;  // as the driver numbers them: 0 = the main one
    settings::Frame frame;                        // the game's window frame, in the game's pixels
    bool elevated = false;                        // this app runs as administrator
};
SystemInfo probeSystem();

// Whether uaRO.exe from this folder runs (spec §4.1); a uaRO.exe whose path cannot be read counts as running.
bool gameRunning(const std::wstring& gameExe);

}  // namespace uagfx::app
