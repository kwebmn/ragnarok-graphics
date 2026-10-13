#pragma once
#include <windows.h>

namespace uagfx::dx7 {

// A method uaRO.exe is not known to call (docs/research/*.md): logged once per name, answered DDERR_UNSUPPORTED.
HRESULT notImplemented(const char* method);

// QueryInterface for an interface we do not offer: logged once per object and interface, so the log names it if
// Bink or ROExt asks a surface for an older interface.
void unknownInterface(const char* object, REFIID riid);

}  // namespace uagfx::dx7
