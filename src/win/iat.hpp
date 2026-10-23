#pragma once
#include <windows.h>

namespace uagfx::win {

// 3.0.1-beta.8: uaRO.exe's import slot of `name` (imported by name) from `dll` (any case), from any DLL when `dll`
// is null; null when it has none. Shared by the sound's hooks (win/sound_hook.cpp), ROExt's (win/roext.cpp) and the
// pad's, which chains after another's hook through it.
IMAGE_THUNK_DATA* importSlot(const char* dll, const char* name);

// The slot's function: `hook` in, what was there kept in `old` - the real one, or another's hook that then chains
bool patchSlot(IMAGE_THUNK_DATA* slot, void* hook, void** old);

template <typename Fn>
bool hookImport(const char* dll, const char* name, Fn hook, Fn& old) {
    void* was = nullptr;
    if (!patchSlot(importSlot(dll, name), reinterpret_cast<void*>(hook), &was)) return false;
    old = reinterpret_cast<Fn>(was);
    return true;
}

template <typename Fn>
void readImport(const char* dll, const char* name, Fn& out) {
    if (IMAGE_THUNK_DATA* slot = importSlot(dll, name)) out = reinterpret_cast<Fn>(slot->u1.Function);
}

}  // namespace uagfx::win
