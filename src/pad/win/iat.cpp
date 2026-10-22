#include "pad/win/iat.hpp"

namespace uapad::iat {

void* patch(HMODULE module, void* target, void* replacement) {
    auto* base = reinterpret_cast<BYTE*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return nullptr;
    bool patched = false;
    for (auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp) {
        for (auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk); thunk->u1.Function; ++thunk) {
            if (reinterpret_cast<void*>(thunk->u1.Function) != target) continue;
            DWORD old = 0;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &old)) continue;
            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), old, &old);
            patched = true;
        }
    }
    return patched ? target : nullptr;
}

}  // namespace uapad::iat
