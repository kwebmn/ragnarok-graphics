#include "pad/win/client_memory.hpp"

namespace uapad::clientmem {

bool readable(uintptr_t p, size_t n) {
    if (p == 0 || n == 0 || p + n < p) return false;
    const DWORD kRead = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                        PAGE_EXECUTE_WRITECOPY;
    for (uintptr_t at = p; at < p + n;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(at), &mbi, sizeof mbi) == 0) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !(mbi.Protect & kRead))
            return false;
        at = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}

bool writable(uintptr_t p, size_t n) {
    if (p == 0 || n == 0 || p + n < p) return false;
    const DWORD kWrite = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    for (uintptr_t at = p; at < p + n;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(at), &mbi, sizeof mbi) == 0) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !(mbi.Protect & kWrite))
            return false;
        at = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}

bool matches(const uint8_t* sig, size_t n, uintptr_t at) {
    return readable(at, n) && std::memcmp(reinterpret_cast<const void*>(at), sig, n) == 0;
}

}  // namespace uapad::clientmem
