// 3.0.1-beta.5 (the user: our own libraries in place of the client's old ones): cps.dll - zlib's compress and
// uncompress, which uaRO.exe takes by name (cdecl) for its GRF archives' files, over miniz (the driver's own GRF
// reader's inflate since 0.42) in place of the client's zlib 1.x of the 2000s; 3.0.2-beta.11 over libdeflate (the
// game's files inflated some 6 times as fast). The return codes are zlib's.
#include <windows.h>

#include "core/client_libs.hpp"

extern "C" {

// Told from the game's own cps.dll by these bytes (core/client_libs.hpp)
__attribute__((used)) const char kUaroMark[] = "uaRO Graphics client library: cps.dll (zlib over libdeflate)";

// The stream of `source` into `dest`; *destLen its room in, the stream's length out (Z_OK, Z_BUF_ERROR, Z_MEM_ERROR)
int compress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen) {
    return uagfx::client_libs::zlibCompress(dest, destLen, source, sourceLen);
}

// 3.0.2-beta.12: what the game has inflated - its files, their bytes and the time in microseconds, each wrapping
// at 2^32 - for the driver's line on a map's loading (the differences over it; uagfx_inflated)
static volatile LONG g_files = 0, g_bytes = 0, g_micros = 0;
static LARGE_INTEGER g_frequency{};

// 3.0.15-beta.7 (R-FPT4KM and a player's «не можу попасти в замок, 2 раз словив це»: an access violation in here
// reading 0x36c6f000 from 0x36c6effd): uaRO.exe gives a GRF entry's length rounded up (its aligned size), and its
// buffer may end on a page's end before that - the old zlib stopped at the stream's end, libdeflate reads ahead up to
// the length it is told. The bytes that can be read from `p`, at most `n` (the pages committed and readable).
static unsigned long readable(const unsigned char* p, unsigned long n) {
    unsigned long have = 0;
    while (have < n) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(p + have, &m, sizeof m)) break;
        const DWORD no = PAGE_NOACCESS | PAGE_GUARD;
        if (m.State != MEM_COMMIT || (m.Protect & no) || !m.Protect) break;
        const auto end = reinterpret_cast<const unsigned char*>(m.BaseAddress) + m.RegionSize;
        have += static_cast<unsigned long>(end - (p + have));
    }
    return have < n ? have : n;
}

// 3.0.15-beta.8 (the same player's third crash: msvcrt's memcpy from in here writing at 0x1192aff0, a page's end):
// the room told may also be more than the buffer has - the bytes that can be written at `p`, at most `n`
static unsigned long writable(unsigned char* p, unsigned long n) {
    unsigned long have = 0;
    while (have < n) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(p + have, &m, sizeof m)) break;
        const DWORD canWrite = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if (m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !(m.Protect & canWrite)) break;
        const auto end = reinterpret_cast<unsigned char*>(m.BaseAddress) + m.RegionSize;
        have += static_cast<unsigned long>(end - (p + have));
    }
    return have < n ? have : n;
}

// Back: *destLen the room in, the data's length out (Z_OK, Z_BUF_ERROR, Z_DATA_ERROR, Z_MEM_ERROR)
int uncompress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen) {
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    if (destLen) *destLen = writable(dest, *destLen);  // .8: past what can be written: Z_BUF_ERROR, not a crash
    const int result = uagfx::client_libs::zlibUncompress(dest, destLen, source, readable(source, sourceLen));
    QueryPerformanceCounter(&b);
    InterlockedIncrement(&g_files);
    if (result == 0 && destLen) InterlockedExchangeAdd(&g_bytes, LONG(*destLen));
    if (g_frequency.QuadPart > 0)
        InterlockedExchangeAdd(&g_micros, LONG((b.QuadPart - a.QuadPart) * 1000000 / g_frequency.QuadPart));
    return result;
}

// 3.0.2-beta.12, the driver's (not uaRO.exe's): the files inflated, their bytes, the microseconds so far
void uagfx_inflated(unsigned int out[3]) {
    out[0] = unsigned(g_files);
    out[1] = unsigned(g_bytes);
    out[2] = unsigned(g_micros);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        volatile const char* mark = kUaroMark;  // kept in the file
        (void)mark;
        QueryPerformanceFrequency(&g_frequency);
    }
    return TRUE;
}

}  // extern "C"
