#include <windows.h>

#include <ddraw.h>

#include <string>

#include "core/renderer_choice.hpp"
#include "win/sound_hook.hpp"
#include "win/context.hpp"
#include "win/dx7/ddraw7.hpp"
#include "win/log.hpp"
#include "win/pad_mount.hpp"
#include "win/roext.hpp"
#include "win/real_ddraw.hpp"
#include "win/window_mode.hpp"

namespace {

template <typename Fn>
Fn real(const char* name) {
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(uagfx::win::realProc(name)));
}

bool g_uaroFailed = false;  // the uaRO renderer did not start: every later call goes to the other DirectDraw

// DirectDraw of the uaRO renderer. S_FALSE: Direct3D 12 would not start, the caller falls back.
HRESULT createUaro(REFIID iid, LPVOID* out, IUnknown* outer) {
    if (outer) return CLASS_E_NOAGGREGATION;
    uagfx::win::Guard guard;
    uagfx::win::Context& c = uagfx::win::Context::get();
    c.config = uagfx::win::startup().config;
    c.settingsChanged();
    uagfx::win::blackenGameWindow();  // before the renderer starts: its start takes a moment
    std::string error;
    if (!c.open(error)) {
        LOG_ERROR("the uaRO renderer could not start (%s): falling back to another DirectDraw", error.c_str());
        return S_FALSE;
    }
    const HRESULT hr = uagfx::dx7::createDirectDraw(iid, out);
    LOG_INFO("DirectDrawCreateEx = 0x%08lx (uaRO renderer)", static_cast<unsigned long>(hr));
    return hr;
}

}  // namespace

// The only DDRAW.dll import of uaRO.exe. ROExt inline-hooks it as soon as the game starts, so it begins
// with the hot-patch prologue (mov edi,edi / push ebp / mov ebp,esp) that hooks expect.
extern "C" __attribute__((ms_hook_prologue)) HRESULT WINAPI Proxy_DirectDrawCreateEx(GUID* guid, LPVOID* out,
                                                                                    REFIID iid, IUnknown* outer) {
    if (uagfx::win::startup().choice.renderer == uagfx::Renderer::Uaro && !g_uaroFailed) {
        const HRESULT hr = createUaro(iid, out, outer);
        if (hr != S_FALSE) return hr;
        g_uaroFailed = true;
    }
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

// The private exports of ddraw.dll. The system's d3dim700.dll imports some of them, and dgVoodoo's D3DImm.dll
// looks them up at run time; both ask for "ddraw.dll" by name and get us, the first module of that name.
namespace {

template <typename Fn>
Fn loaded(const char* name) {
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(uagfx::win::loadedProc(name)));
}

}  // namespace

extern "C" void WINAPI Proxy_AcquireDDThreadLock() {
    using Fn = void(WINAPI*)();
    if (const Fn fn = loaded<Fn>("AcquireDDThreadLock")) fn();
}

extern "C" void WINAPI Proxy_ReleaseDDThreadLock() {
    using Fn = void(WINAPI*)();
    if (const Fn fn = loaded<Fn>("ReleaseDDThreadLock")) fn();
}

extern "C" HRESULT WINAPI Proxy_CompleteCreateSysmemSurface(DWORD a) {
    using Fn = HRESULT(WINAPI*)(DWORD);
    const Fn fn = loaded<Fn>("CompleteCreateSysmemSurface");
    return fn ? fn(a) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_D3DParseUnknownCommand(LPVOID command, LPVOID* next) {
    using Fn = HRESULT(WINAPI*)(LPVOID, LPVOID*);
    const Fn fn = loaded<Fn>("D3DParseUnknownCommand");
    return fn ? fn(command, next) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DDGetAttachedSurfaceLcl(DWORD a, DWORD b, DWORD c) {
    using Fn = HRESULT(WINAPI*)(DWORD, DWORD, DWORD);
    const Fn fn = loaded<Fn>("DDGetAttachedSurfaceLcl");
    return fn ? fn(a, b, c) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DDInternalLock(DWORD a, DWORD b) {
    using Fn = HRESULT(WINAPI*)(DWORD, DWORD);
    const Fn fn = loaded<Fn>("DDInternalLock");
    return fn ? fn(a, b) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DDInternalUnlock(DWORD a) {
    using Fn = HRESULT(WINAPI*)(DWORD);
    const Fn fn = loaded<Fn>("DDInternalUnlock");
    return fn ? fn(a) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DSoundHelp(DWORD a, DWORD b, DWORD c) {
    using Fn = HRESULT(WINAPI*)(DWORD, DWORD, DWORD);
    const Fn fn = loaded<Fn>("DSoundHelp");
    return fn ? fn(a, b, c) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_GetDDSurfaceLocal(DWORD a, DWORD b, DWORD c) {
    using Fn = HRESULT(WINAPI*)(DWORD, DWORD, DWORD);
    const Fn fn = loaded<Fn>("GetDDSurfaceLocal");
    return fn ? fn(a, b, c) : E_FAIL;
}

extern "C" DWORD WINAPI Proxy_GetOLEThunkData(DWORD index) {
    using Fn = DWORD(WINAPI*)(DWORD);
    const Fn fn = loaded<Fn>("GetOLEThunkData");
    return fn ? fn(index) : 0;
}

extern "C" HRESULT WINAPI Proxy_GetSurfaceFromDC(HDC dc, LPDIRECTDRAWSURFACE7* out, DWORD c) {
    using Fn = HRESULT(WINAPI*)(HDC, LPDIRECTDRAWSURFACE7*, DWORD);
    const Fn fn = loaded<Fn>("GetSurfaceFromDC");
    return fn ? fn(dc, out, c) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_RegisterSpecialCase(DWORD a, DWORD b, DWORD c, DWORD d) {
    using Fn = HRESULT(WINAPI*)(DWORD, DWORD, DWORD, DWORD);
    const Fn fn = loaded<Fn>("RegisterSpecialCase");
    return fn ? fn(a, b, c, d) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_SetAppCompatData(DWORD a, DWORD b) {
    using Fn = HRESULT(WINAPI*)(DWORD, DWORD);
    const Fn fn = loaded<Fn>("SetAppCompatData");
    return fn ? fn(a, b) : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        uagfx::win::installSoundHooks();  // 0.91: before the game opens its sound (its import slots only)
        uagfx::win::mountPad();           // 2.9: uaRO Pad, before the game makes its DirectInput
        uagfx::win::installRoext();       // 3.0.1-beta.8: ROExt's work, before the game makes its window
    }
    if (reason == DLL_PROCESS_DETACH) {
        uagfx::win::logModulesAtExit();
        uagfx::win::unmountPad();
    }
    return TRUE;
}
