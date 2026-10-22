#pragma once
#include <windows.h>

namespace uapad::iat {

// Replaces every import-table slot of `module` that currently points to `target` with `replacement`.
// Matching by address works for imports by name and by ordinal (the client imports WS2_32 by ordinal).
// Returns `target` if at least one slot was patched, nullptr otherwise.
void* patch(HMODULE module, void* target, void* replacement);

}  // namespace uapad::iat
