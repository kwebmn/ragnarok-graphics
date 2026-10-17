#pragma once
#include <windows.h>

#include "core/config.hpp"
#include "core/renderer_choice.hpp"

namespace uagfx::win {

// What the first call of any export decides, once: the settings, the log and the renderer.
struct Startup {
    Config config;
    RendererChoice choice;
    int cardLevel = 1;  // 2.2: the card's for the automatic effects set (effect_groups::cardLevel)
    // 2.2.1: the card's name, maker and video memory (the statistics, core/usage.hpp)
    std::string adapter;
    unsigned vendor = 0;
    unsigned long long vramMb = 0;
};
const Startup& startup();

// The system's DirectDraw: where the calls go when the uaRO renderer is not used, and its fallback (2.2: dgVoodoo is
// gone). Loaded on the first call that needs it, never in DllMain (no LoadLibrary under the loader lock).
HMODULE realModule();
FARPROC realProc(const char* name);  // nullptr when the module or the function is missing
// The same, but never starts loading: for the private exports that d3dim700.dll calls
// while the real DirectDraw is being set up. nullptr until something has loaded it.
FARPROC loadedProc(const char* name);
// Called from DllMain on DLL_PROCESS_DETACH: which DirectDraw modules the game ended up with.
void logModulesAtExit();

}  // namespace uagfx::win
