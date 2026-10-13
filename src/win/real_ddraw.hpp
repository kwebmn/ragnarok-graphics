#pragma once
#include <windows.h>

#include "core/config.hpp"
#include "core/renderer_choice.hpp"

namespace uagfx::win {

// What the first call of any export decides, once: the settings, the log and the renderer.
struct Startup {
    Config config;
    RendererChoice choice;
};
const Startup& startup();

// The DirectDraw calls go to when the uaRO renderer is not used: dgVoodoo or the system one. With the uaRO
// renderer chosen it is the fallback (dgVoodoo when it is there). Loaded on the first call that needs it, never in
// DllMain (no LoadLibrary under the loader lock).
HMODULE realModule();
FARPROC realProc(const char* name);  // nullptr when the module or the function is missing
// The same, but never starts loading: for the private exports that d3dim700.dll / dgVoodoo's D3DImm.dll call
// while the real DirectDraw is being set up. nullptr until something has loaded it.
FARPROC loadedProc(const char* name);
// Called from DllMain on DLL_PROCESS_DETACH: which DirectDraw modules the game ended up with.
void logModulesAtExit();

}  // namespace uagfx::win
