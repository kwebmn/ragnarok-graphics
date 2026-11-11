#include "pad/win/client_memory.hpp"

#include "win/safe_read.hpp"

namespace uapad::clientmem {
namespace {

// 3.0.18-beta.161 (the owner: «делай 1 и 2»): each check below is a call into Windows (VirtualQuery) - counted on the
// game's thread, its time summed, for the driver's 30 s line
DWORD g_countedThread = 0;
uint32_t g_queries = 0;
LONGLONG g_ticks = 0;

SIZE_T query(uintptr_t at, MEMORY_BASIC_INFORMATION& mbi) {
    if (!g_countedThread || GetCurrentThreadId() != g_countedThread)
        return VirtualQuery(reinterpret_cast<LPCVOID>(at), &mbi, sizeof mbi);
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    const SIZE_T got = VirtualQuery(reinterpret_cast<LPCVOID>(at), &mbi, sizeof mbi);
    QueryPerformanceCounter(&b);
    ++g_queries;
    g_ticks += b.QuadPart - a.QuadPart;
    return got;
}

}  // namespace

void countOn(DWORD gameThread) { g_countedThread = gameThread; }

void takeQueries(uint32_t& count, double& ms) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    count = g_queries;
    ms = f.QuadPart ? double(g_ticks) * 1000.0 / double(f.QuadPart) : 0.0;
    g_queries = 0;
    g_ticks = 0;
}

bool readable(uintptr_t p, size_t n) {  // 3.0.18-beta.165: win/safe_read's - no VirtualQuery each time
    return p + n >= p && uagfx::win::saferead::readable(uint32_t(p), n);
}

bool copy(uintptr_t p, void* out, size_t n) {
    if (p == 0 || n == 0 || p + n < p) return false;
    if (uagfx::win::saferead::ready()) return uagfx::win::saferead::copy(out, uint32_t(p), n);
    if (!readable(p, n)) return false;
    std::memcpy(out, reinterpret_cast<const void*>(p), n);
    return true;
}

bool writable(uintptr_t p, size_t n) {
    if (p == 0 || n == 0 || p + n < p) return false;
    const DWORD kWrite = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    for (uintptr_t at = p; at < p + n;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (query(at, mbi) == 0) return false;
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
