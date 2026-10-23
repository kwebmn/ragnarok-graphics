#include "win/iat.hpp"

#include <cstdint>
#include <cstring>

namespace uagfx::win {

IMAGE_THUNK_DATA* importSlot(const char* dll, const char* name) {
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress || !name) return nullptr;
    for (auto* d = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        if ((dll && lstrcmpiA(reinterpret_cast<const char*>(base + d->Name), dll) != 0) || !d->OriginalFirstThunk)
            continue;
        auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + d->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            const auto* by = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(by->Name), name) == 0) return slots;
        }
    }
    return nullptr;
}

bool patchSlot(IMAGE_THUNK_DATA* slot, void* hook, void** old) {
    DWORD was = 0;
    if (!slot || !VirtualProtect(&slot->u1.Function, sizeof(void*), PAGE_READWRITE, &was)) return false;
    *old = reinterpret_cast<void*>(slot->u1.Function);
    slot->u1.Function = reinterpret_cast<uintptr_t>(hook);
    DWORD unused = 0;
    VirtualProtect(&slot->u1.Function, sizeof(void*), was, &unused);
    return true;
}

}  // namespace uagfx::win
