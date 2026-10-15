#include "win/real_ddraw.hpp"

#include <string>

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

// 0.2.0 has the uaRO renderer. It is opt-in (backend = uaro); dgVoodoo stays the default.
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
    LOG_INFO("report: the settings app watches this game ([report] auto = 1)");
}

// dgVoodoo's DDraw.dll in the dgvoodoo folder; its D3DImm.dll sits next to it, so that folder is added to the
// DLL search path for the rest of the process.
HMODULE loadDgVoodoo(const std::string& dir) {
    const std::string folder = dir + "\\dgvoodoo";
    if (!SetDllDirectoryA(folder.c_str())) LOG_ERROR("SetDllDirectory(%s) failed (error %lu)", folder.c_str(), GetLastError());
    const std::string path = folder + "\\DDraw.dll";
    HMODULE m = LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m) LOG_ERROR("cannot load %s (error %lu)", path.c_str(), GetLastError());
    return m;
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
    std::string ini;
    const bool haveIni = readTextFile(dir + "\\uaro-graphics.ini", ini);
    s.config = parseConfig(ini);
    wlog::open(dir + "\\uaro-graphics.log", s.config.logLevel);
    installCrashHandler(dir);  // 0.52: where a crash happened, and a minidump
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    LOG_INFO("settings: %s; backend = %s, api = %s, mode = %s",
             haveIni ? "uaro-graphics.ini" : "no uaro-graphics.ini, defaults", backendToken(s.config.backend),
             apiToken(s.config.api), displayModeToken(s.config.mode));
    LOG_INFO("display: high_resolution = %d, ui_filter = %s, render_scale = %d%%, sharpen = %d",
             s.config.highResolution ? 1 : 0, uiFilterToken(s.config.uiFilter), s.config.renderScale, s.config.sharpen);
    const WindowsVersion v = windowsVersion();
    LOG_INFO("Windows %lu.%lu build %lu", v.major, v.minor, v.build);
    if (!s.config.gpu.empty()) LOG_INFO("graphics card chosen: %s ([renderer] gpu)", s.config.gpu.c_str());
    logModule("dinput.dll");   // ROExt or uaRO Pad
    logModule("dinput8.dll");
    // The game's window black before anything that takes time - the Direct3D 12 probe, the renderer's start - so
    // that its white 640x480 start-up area does not stay on the screen meanwhile (window_mode.hpp).
    if (s.config.backend == Backend::Uaro) blackenGameWindow();

    gpu::setChosenAdapter(s.config.gpu);  // 0.50.2: the probes look at the card the renderer will take
    SystemFacts facts;
    facts.windows10 = v.major >= 10;
    facts.dgvoodoo = fileExists(dir + "\\dgvoodoo\\DDraw.dll");
    if (facts.windows10) {  // always: backend = uaro needs it, and the log names the adapter either way
        const D3D12Probe p = probeD3D12();
        facts.d3d12 = p.ok;
        LOG_INFO("Direct3D 12: %s, adapter %s (vendor 0x%04x, device 0x%04x, %llu MB), hr 0x%08lx",
                 p.ok ? "OK" : "FAILED", p.adapter.c_str(), p.vendor, p.device, p.vramMb,
                 static_cast<unsigned long>(p.hr));
    }
    // 0.46: Direct3D 11 - asked for, or (auto) where Direct3D 12 is not there.
    if (s.config.backend == Backend::Uaro &&
        (s.config.api == Api::D3d11 || (s.config.api == Api::Auto && !(facts.windows10 && facts.d3d12)))) {
        const D3D11Probe p = probeD3D11();
        facts.d3d11 = p.ok;
        LOG_INFO("Direct3D 11: %s, adapter %s, feature level %s, hr 0x%08lx", p.ok ? "OK" : "FAILED",
                 p.adapter.c_str(), p.ok ? featureLevelName(p.level).c_str() : "-", static_cast<unsigned long>(p.hr));
    }
    s.choice = chooseRenderer(s.config, facts, kUaroBuilt);
    if (s.config.reportAuto && s.choice.renderer == Renderer::Uaro) startReportWatcher(dir);
    LOG_INFO("renderer: %s%s (%s)", rendererName(s.choice.renderer),
             s.choice.renderer == Renderer::Uaro && s.choice.api == Api::D3d11 ? " with Direct3D 11" : "",
             s.choice.reason.c_str());
    return s;
}

HMODULE loadReal() {
    const Startup& s = startup();
    const std::string dir = gameDir();
    HMODULE m = nullptr;
    // dgVoodoo when it was chosen, and as the uaRO renderer's fallback when it is there.
    if (s.choice.renderer != Renderer::System && fileExists(dir + "\\dgvoodoo\\DDraw.dll")) m = loadDgVoodoo(dir);
    if (!m) m = loadSystem();
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
    logModule("D3DImm.dll");  // which D3DImm.dll dgVoodoo's DirectDraw picked up
    logModule("ddraw.dll");
}

}  // namespace uagfx::win
