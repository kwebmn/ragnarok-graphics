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

// Back: *destLen the room in, the data's length out (Z_OK, Z_BUF_ERROR, Z_DATA_ERROR, Z_MEM_ERROR)
int uncompress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen) {
    return uagfx::client_libs::zlibUncompress(dest, destLen, source, sourceLen);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        volatile const char* mark = kUaroMark;  // kept in the file
        (void)mark;
    }
    return TRUE;
}

}  // extern "C"
