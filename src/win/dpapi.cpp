#include "win/dpapi.hpp"

#include <windows.h>

namespace uagfx::win::dpapi {
namespace {

struct Blob {  // crypt32's DATA_BLOB
    DWORD size;
    BYTE* data;
};
using CryptFn = BOOL(WINAPI*)(Blob* in, const wchar_t* name, Blob* entropy, void* reserved, void* prompt, DWORD flags,
                              Blob* out);
constexpr DWORD kNoUi = 0x1;  // CRYPTPROTECT_UI_FORBIDDEN
constexpr char kEntropy[] = "uaRO Graphics - the saved accounts";

CryptFn crypt(const char* name) {
    static const HMODULE m = LoadLibraryA("crypt32.dll");
    return reinterpret_cast<CryptFn>(reinterpret_cast<void*>(m ? GetProcAddress(m, name) : nullptr));
}

bool run(const char* fn, const std::string& in, std::string& out) {
    const CryptFn f = crypt(fn);
    if (!f) return false;
    Blob i{DWORD(in.size()), reinterpret_cast<BYTE*>(const_cast<char*>(in.data()))};
    Blob e{DWORD(sizeof kEntropy - 1), reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy))};
    Blob o{0, nullptr};
    if (!f(&i, nullptr, &e, nullptr, nullptr, kNoUi, &o) || !o.data) return false;
    out.assign(reinterpret_cast<const char*>(o.data), o.size);
    SecureZeroMemory(o.data, o.size);
    LocalFree(o.data);
    return true;
}

}  // namespace

bool protect(const std::string& plain, std::string& blob) { return run("CryptProtectData", plain, blob); }
bool unprotect(const std::string& blob, std::string& plain) { return run("CryptUnprotectData", blob, plain); }

}  // namespace uagfx::win::dpapi
