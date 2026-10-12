#pragma once
#include <windows.h>

namespace uagfx::win {

// The DirectDraw every call is passed to in this version. Loaded on the first call of any export,
// never in DllMain (no LoadLibrary under the loader lock).
HMODULE realModule();
FARPROC realProc(const char* name);  // nullptr when the module or the function is missing

}  // namespace uagfx::win
