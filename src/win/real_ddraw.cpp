#include "win/real_ddraw.hpp"

#include <string>

#include "core/config.hpp"
#include "core/renderer_choice.hpp"
#include "core/version.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/system_probe.hpp"

namespace uagfx::win {
namespace {

HMODULE g_loaded = nullptr;  // set once the real DirectDraw is loaded

// 0.1.x has no uaRO renderer yet: everything goes to dgVoodoo or the system DirectDraw.
constexpr bool kUaroBuilt = false;

void logModule(const char* name) {
    if (HMODULE m = GetModuleHandleA(name)) LOG_INFO("%s: %s", name, modulePath(m).c_str());
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

HMODULE loadReal() {
    const std::string dir = gameDir();
    std::string ini;
    const bool haveIni = readTextFile(dir + "\\uaro-graphics.ini", ini);
    const Config config = parseConfig(ini);
    wlog::open(dir + "\\uaro-graphics.log", config.logLevel);
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    LOG_INFO("settings: %s; backend = %s, mode = %s", haveIni ? "uaro-graphics.ini" : "no uaro-graphics.ini, defaults",
             backendToken(config.backend), displayModeToken(config.mode));
    const WindowsVersion v = windowsVersion();
    LOG_INFO("Windows %lu.%lu build %lu", v.major, v.minor, v.build);
    logModule("dinput.dll");   // ROExt or uaRO Pad
    logModule("dinput8.dll");

    SystemFacts facts;
    facts.windows10 = v.major >= 10;
    facts.dgvoodoo = fileExists(dir + "\\dgvoodoo\\DDraw.dll");
    if (config.backend == Backend::Uaro && facts.windows10) {
        const D3D12Probe p = probeD3D12();
        facts.d3d12 = p.ok;
        LOG_INFO("Direct3D 12: %s, adapter %s (vendor 0x%04x, device 0x%04x, %llu MB), hr 0x%08lx",
                 p.ok ? "OK" : "FAILED", p.adapter.c_str(), p.vendor, p.device, p.vramMb,
                 static_cast<unsigned long>(p.hr));
    }
    const RendererChoice choice = chooseRenderer(config, facts, kUaroBuilt);
    LOG_INFO("renderer: %s (%s)", rendererName(choice.renderer), choice.reason.c_str());

    HMODULE m = nullptr;
    if (choice.renderer == Renderer::DgVoodoo) m = loadDgVoodoo(dir);
    if (!m) m = loadSystem();
    if (m) LOG_INFO("passing DirectDraw to %s", modulePath(m).c_str());
    g_loaded = m;
    return m;
}

}  // namespace

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
