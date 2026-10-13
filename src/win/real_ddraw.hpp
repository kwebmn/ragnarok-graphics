#pragma once
#include <windows.h>

namespace uagfx::win {

// The DirectDraw every call is passed to in this version. Loaded on the first call of any export,
// never in DllMain (no LoadLibrary under the loader lock).
HMODULE realModule();
FARPROC realProc(const char* name);  // nullptr when the module or the function is missing
// The same, but never starts loading: for the private exports that d3dim700.dll / dgVoodoo's D3DImm.dll call
// while the real DirectDraw is being set up. nullptr until DirectDrawCreateEx has loaded it.
FARPROC loadedProc(const char* name);
// Called from DllMain on DLL_PROCESS_DETACH: which DirectDraw modules the game ended up with.
void logModulesAtExit();

}  // namespace uagfx::win
