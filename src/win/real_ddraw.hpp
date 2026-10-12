#pragma once
#include <windows.h>

namespace uagfx::win {

// The DirectDraw every call is passed to in this version. Loaded on the first call of any export,
// never in DllMain (no LoadLibrary under the loader lock).
HMODULE realModule();
FARPROC realProc(const char* name);  // nullptr when the module or the function is missing
// Called from DllMain on DLL_PROCESS_DETACH: which DirectDraw modules the game ended up with.
void logModulesAtExit();

}  // namespace uagfx::win
