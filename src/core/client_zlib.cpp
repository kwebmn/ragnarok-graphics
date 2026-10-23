#include "core/client_libs.hpp"

// cps.dll's zlib in a file of its own: the DLL links this and libdeflate alone (core/client_libs.hpp). 3.0.2-beta.11
// (the client build session): libdeflate in place of miniz - UaRO.grf's 25 928 files inflated 5.9 times as fast
// (2141 MB/s against 365 on a Ryzen 7 8700GE, 64-bit), the same bytes; the two damaged ones refused by both.
#include "libdeflate.h"

namespace uagfx::client_libs {
namespace {

constexpr int kOk = 0, kDataError = -3, kMemError = -4, kBufError = -5;  // zlib's

}  // namespace

int zlibCompress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen) {
    libdeflate_compressor* c = libdeflate_alloc_compressor(6);  // zlib's default level
    if (!c) return kMemError;
    const size_t n = libdeflate_zlib_compress(c, source, sourceLen, dest, *destLen);
    libdeflate_free_compressor(c);
    if (!n) return kBufError;
    *destLen = static_cast<unsigned long>(n);
    return kOk;
}

int zlibUncompress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen) {
    libdeflate_decompressor* d = libdeflate_alloc_decompressor();  // one a call: the game may call from any thread
    if (!d) return kMemError;
    size_t n = 0;  // the length taken out; bytes after the stream are not read (as zlib's)
    const libdeflate_result r = libdeflate_zlib_decompress(d, source, sourceLen, dest, *destLen, &n);
    libdeflate_free_decompressor(d);
    if (r == LIBDEFLATE_INSUFFICIENT_SPACE) return kBufError;
    if (r != LIBDEFLATE_SUCCESS) return kDataError;
    *destLen = static_cast<unsigned long>(n);
    return kOk;
}

}  // namespace uagfx::client_libs
