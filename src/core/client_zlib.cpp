#include "core/client_libs.hpp"

// cps.dll's zlib in a file of its own: the DLL links this and miniz alone (core/client_libs.hpp)
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

namespace uagfx::client_libs {

int zlibCompress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen) {
    mz_ulong n = *destLen;
    const int r = mz_compress(dest, &n, source, mz_ulong(sourceLen));
    *destLen = static_cast<unsigned long>(n);
    return r;
}

int zlibUncompress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen) {
    mz_ulong n = *destLen;
    const int r = mz_uncompress(dest, &n, source, mz_ulong(sourceLen));
    *destLen = static_cast<unsigned long>(n);
    return r;
}

}  // namespace uagfx::client_libs
