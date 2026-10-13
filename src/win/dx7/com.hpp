#pragma once
#include <windows.h>

namespace uagfx::dx7 {

// A method uaRO.exe is not known to call (docs/research/*.md): logged once per name, answered DDERR_UNSUPPORTED.
HRESULT notImplemented(const char* method);

}  // namespace uagfx::dx7
