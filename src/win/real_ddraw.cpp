#include "win/real_ddraw.hpp"

#include "core/layout_migration.hpp"
#include "win/disk.hpp"

#include <string>

#include "core/effect_groups.hpp"
#include "core/settings/report.hpp"
#include "core/version.hpp"
#include "win/crash.hpp"
#include "win/gpu/adapters.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/system_probe.hpp"
#include "win/window_mode.hpp"

namespace uagfx::win {
namespace {

HMODULE g_loaded = nullptr;  // set once the real DirectDraw is loaded

// 0.2.0 has the uaRO renderer; since 2.2 it is the only one (dgVoodoo is gone).
constexpr bool kUaroBuilt = true;

void logModule(const char* name) {
    if (HMODULE m = GetModuleHandleA(name)) LOG_INFO("%s: %s", name, modulePath(m).c_str());
}

// 0.51: [report] auto - the settings app waits for this game to end and, when its log says it went badly, sends a
// report to the developers (app/report_send.hpp). The game itself never goes to the network.
void startReportWatcher(const std::string& dir) {
    const std::string exe = dir + "\\uaro-graphics.exe";
    if (!fileExists(exe)) {
        LOG_INFO("report: uaro-graphics.exe is not next to the game - no report when something goes wrong");
        return;
    }
    std::string line = "\"" + exe + "\" --watch " + std::to_string(GetCurrentProcessId());
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(exe.c_str(), &line[0], nullptr, nullptr, FALSE, BELOW_NORMAL_PRIORITY_CLASS, nullptr,
                        dir.c_str(), &si, &pi)) {
        LOG_ERROR("report: the watcher would not start (error %lu)", GetLastError());
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    LOG_INFO("report: the settings app watches this game ([report] auto = 1 or [stats] send = 1)");
}

HMODULE loadSystem() {
    const std::string path = systemDir() + "\\ddraw.dll";
    HMODULE m = LoadLibraryA(path.c_str());
    if (!m) LOG_ERROR("cannot load %s (error %lu)", path.c_str(), GetLastError());
    return m;
}

Startup decide() {
    Startup s;
    const std::string dir = gameDir();
    // 2.8: a 2.x install's files into uaro-graphics\ before anything reads them (core/layout_migration)
    WinDisk disk;
    const std::vector<std::string> moved = layout::migrate(dir, disk);
    std::string ini;
    const std::string iniPath = ourFile(dir, layout::File::Ini);
    const bool haveIni = readTextFile(iniPath, ini);
    s.config = parseConfig(ini);
    wlog::open(layout::join(dir, layout::now(layout::File::DriverLog)), s.config.logLevel);
    installCrashHandler(dir);  // 0.52: where a crash happened, and a minidump
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    for (const std::string& line : moved) LOG_INFO("%s", line.c_str());
    makeFolders(layout::join(dir, layout::kCache));
    LOG_INFO("settings: %s; api = %s, mode = %s", haveIni ? iniPath.c_str() : "no uaro-graphics.ini, defaults",
             apiToken(s.config.api), displayModeToken(s.config.mode));
    LOG_INFO("display: high_resolution = %d, ui_filter = %s, render_scale = %d%%, sharpen = %d",
             s.config.highResolution ? 1 : 0, uiFilterToken(s.config.uiFilter), s.config.renderScale, s.config.sharpen);
    const WindowsVersion v = windowsVersion();
    LOG_INFO("Windows %lu.%lu build %lu", v.major, v.minor, v.build);
    s.computer = probeComputer();  // 2.5.15: the rest of the computer, for the reports and the statistics
    LOG_INFO("processor: %s; memory: %s; power: %s", processorText(s.computer).c_str(),
             memoryText(s.computer).c_str(), powerText(s.computer).c_str());
    if (!s.config.gpu.empty()) LOG_INFO("graphics card chosen: %s ([renderer] gpu)", s.config.gpu.c_str());
    logModule("dinput.dll");   // ROExt or uaRO Pad
    logModule("dinput8.dll");
    // The game's window black before anything that takes time - the Direct3D 12 probe, the renderer's start - so
    // that its white 640x480 start-up area does not stay on the screen meanwhile (window_mode.hpp).
    blackenGameWindow();

    gpu::setChosenAdapter(s.config.gpu);  // 0.50.2: the probes look at the card the renderer will take
    // 2.2.3: DXGI's 3072 MB in this 32-bit process is only a floor - the display driver's own number (the settings
    // app's since 0.51.1): every newer card came out weak, the automatic set the weak one while the app said strong
    const auto realMb = [](unsigned long long mb, const std::string& adapter) {
        if (mb != settings::kVramSeenAtMost) return mb;
        const unsigned long long real = settings::realVramMb(mb, registryVramMb(adapter));
        if (real != mb) LOG_INFO("video memory: %llu MB (the display driver's; DXGI tells a 32-bit program no more "
                                 "than %llu)", real, mb);
        return real;
    };
    SystemFacts facts;
    facts.windows10 = v.major >= 10;
    unsigned long long vramMb = 0;
    if (facts.windows10) {  // the log names the adapter either way
        const D3D12Probe p = probeD3D12();
        facts.d3d12 = p.ok;
        s.adapter = p.adapter;
        s.vendor = p.vendor;
        LOG_INFO("Direct3D 12: %s, adapter %s (vendor 0x%04x, device 0x%04x, %llu MB), hr 0x%08lx",
                 p.ok ? "OK" : "FAILED", p.adapter.c_str(), p.vendor, p.device, p.vramMb,
                 static_cast<unsigned long>(p.hr));
        vramMb = s.vramMb = realMb(p.vramMb, p.adapter);
    }
    // 0.46: Direct3D 11 - asked for, or (auto) where Direct3D 12 is not there.
    if (s.config.api == Api::D3d11 || (s.config.api == Api::Auto && !(facts.windows10 && facts.d3d12))) {
        const D3D11Probe p = probeD3D11();
        facts.d3d11 = p.ok;
        if (s.adapter.empty() || !facts.d3d12) {
            s.adapter = p.adapter;
            s.vendor = p.vendor;
            s.vramMb = realMb(p.vramMb, p.adapter);
        }
        LOG_INFO("Direct3D 11: %s, adapter %s, feature level %s, hr 0x%08lx", p.ok ? "OK" : "FAILED",
                 p.adapter.c_str(), p.ok ? featureLevelName(p.level).c_str() : "-", static_cast<unsigned long>(p.hr));
    }
    s.choice = chooseRenderer(s.config, facts, kUaroBuilt);
    s.cardLevel = effect_groups::cardLevel(s.choice.api == Api::D3d12 && facts.d3d12, vramMb);
    LOG_INFO("effects: set %s, the card's level %d", effect_groups::presetToken(s.config.effects.preset),
             s.cardLevel);
    // 2.2.1: also for the statistics - sent when the game ends, with the player's leave
    if ((s.config.reportAuto || s.config.statsSend) && s.choice.renderer == Renderer::Uaro) startReportWatcher(dir);
    LOG_INFO("renderer: %s%s (%s)", rendererName(s.choice.renderer),
             s.choice.renderer == Renderer::Uaro && s.choice.api == Api::D3d11 ? " with Direct3D 11" : "",
             s.choice.reason.c_str());
    return s;
}

HMODULE loadReal() {
    HMODULE m = loadSystem();  // 2.2: the system's only - dgVoodoo is no longer passed to
    if (m) LOG_INFO("passing DirectDraw to %s", modulePath(m).c_str());
    g_loaded = m;
    return m;
}

}  // namespace

const Startup& startup() {
    static const Startup s = decide();
    return s;
}

HMODULE realModule() {
    static HMODULE m = loadReal();
    return m;
}

FARPROC realProc(const char* name) {
    HMODULE m = realModule();
    return m ? GetProcAddress(m, name) : nullptr;
}

FARPROC loadedProc(const char* name) { return g_loaded ? GetProcAddress(g_loaded, name) : nullptr; }

void logModulesAtExit() {
    logModule("ddraw.dll");
}

}  // namespace uagfx::win
