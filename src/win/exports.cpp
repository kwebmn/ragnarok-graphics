#include <windows.h>

#include <ddraw.h>

#include "win/log.hpp"
#include "win/real_ddraw.hpp"

namespace {

template <typename Fn>
Fn real(const char* name) {
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(uagfx::win::realProc(name)));
}

}  // namespace

// The only DDRAW.dll import of uaRO.exe. ROExt inline-hooks it as soon as the game starts, so it begins
// with the hot-patch prologue (mov edi,edi / push ebp / mov ebp,esp) that hooks expect.
extern "C" __attribute__((ms_hook_prologue)) HRESULT WINAPI Proxy_DirectDrawCreateEx(GUID* guid, LPVOID* out,
                                                                                    REFIID iid, IUnknown* outer) {
    using Fn = HRESULT(WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
    const Fn fn = real<Fn>("DirectDrawCreateEx");
    if (!fn) return E_FAIL;
    const HRESULT hr = fn(guid, out, iid, outer);
    LOG_INFO("DirectDrawCreateEx = 0x%08lx", static_cast<unsigned long>(hr));
    return hr;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawCreate(GUID* guid, LPDIRECTDRAW* out, IUnknown* outer) {
    using Fn = HRESULT(WINAPI*)(GUID*, LPDIRECTDRAW*, IUnknown*);
    const Fn fn = real<Fn>("DirectDrawCreate");
    return fn ? fn(guid, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawCreateClipper(DWORD flags, LPDIRECTDRAWCLIPPER* out, IUnknown* outer) {
    using Fn = HRESULT(WINAPI*)(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*);
    const Fn fn = real<Fn>("DirectDrawCreateClipper");
    return fn ? fn(flags, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawEnumerateA(LPDDENUMCALLBACKA cb, LPVOID ctx) {
    using Fn = HRESULT(WINAPI*)(LPDDENUMCALLBACKA, LPVOID);
    const Fn fn = real<Fn>("DirectDrawEnumerateA");
    return fn ? fn(cb, ctx) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawEnumerateW(LPDDENUMCALLBACKW cb, LPVOID ctx) {
    using Fn = HRESULT(WINAPI*)(LPDDENUMCALLBACKW, LPVOID);
    const Fn fn = real<Fn>("DirectDrawEnumerateW");
    return fn ? fn(cb, ctx) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA cb, LPVOID ctx, DWORD flags) {
    using Fn = HRESULT(WINAPI*)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
    const Fn fn = real<Fn>("DirectDrawEnumerateExA");
    return fn ? fn(cb, ctx, flags) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW cb, LPVOID ctx, DWORD flags) {
    using Fn = HRESULT(WINAPI*)(LPDDENUMCALLBACKEXW, LPVOID, DWORD);
    const Fn fn = real<Fn>("DirectDrawEnumerateExW");
    return fn ? fn(cb, ctx, flags) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DllCanUnloadNow() {
    using Fn = HRESULT(WINAPI*)();
    const Fn fn = real<Fn>("DllCanUnloadNow");
    return fn ? fn() : S_FALSE;
}

extern "C" HRESULT WINAPI Proxy_DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* out) {
    using Fn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    const Fn fn = real<Fn>("DllGetClassObject");
    return fn ? fn(clsid, iid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    return TRUE;
}
