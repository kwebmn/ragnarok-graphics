#include "core/client_libs.hpp"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

namespace uagfx::client_libs {

const std::vector<Lib>& all() {
    static const std::vector<Lib> libs = {
        {"cps.dll", "cps"},       // zlib's compress and uncompress (the GRF archives' files)
        {"binkw32.dll", "bink"},  // RAD's Bink video: none in uaRO
    };
    return libs;
}

bool ours(const std::string& bytes) { return bytes.find("uaRO Graphics client library") != std::string::npos; }

Step decide(bool wanted, bool haveOurs, Root root, bool kept) {
    if (wanted && haveOurs) return root == Root::Ours ? Step::Nothing : Step::PutOurs;
    const bool ourOrNone = root == Root::Ours || root == Root::OursOther || root == Root::Missing;
    return ourOrNone && kept ? Step::RestoreOriginal : Step::Nothing;
}

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
