#include "win/game_memory.hpp"

#include <windows.h>

#include "win/safe_read.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's image [from, to): read once from its headers
void exeImage(uint32_t& from, uint32_t& to) {
    static uint32_t base = 0, end = 0;
    if (!base) {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
        if (!dos) return;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(uintptr_t(dos) + uintptr_t(dos->e_lfanew));
        end = uint32_t(uintptr_t(dos)) + nt->OptionalHeader.SizeOfImage;
        base = uint32_t(uintptr_t(dos));
    }
    from = base;
    to = end;
}

}  // namespace

bool writeGame(uint32_t at, const void* in, size_t size) {
    if (!at || !size) return false;
    uint32_t from = 0, to = 0;
    exeImage(from, to);
    const bool image = at >= from && at < to;
    if (!image && saferead::ready() && saferead::put(at, in, size)) return true;
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), in, size, &n) && n == size;
}

}  // namespace uagfx::win
