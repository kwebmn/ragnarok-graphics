#pragma once
#include <windows.h>

#include <ddraw.h>

#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

// IDirectDraw7 of the uaRO renderer: creates our surfaces and clippers, keeps the display mode the client asked
// for (the monitor's own mode never changes) and its window. The reference count is the client's alone: unless the
// last Release returns 0 the client shows "DDraw object is still referenced" (surfaces-present-caps.md §7).
class DirectDraw7 final : public DirectDraw7Base {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE CreateClipper(DWORD flags, LPDIRECTDRAWCLIPPER* out, IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE CreateSurface(LPDDSURFACEDESC2 desc, LPDIRECTDRAWSURFACE7* out, IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE FlipToGDISurface() override;
    HRESULT STDMETHODCALLTYPE GetCaps(LPDDCAPS driverCaps, LPDDCAPS helCaps) override;
    HRESULT STDMETHODCALLTYPE GetDisplayMode(LPDDSURFACEDESC2 desc) override;
    HRESULT STDMETHODCALLTYPE RestoreAllSurfaces() override;
    HRESULT STDMETHODCALLTYPE RestoreDisplayMode() override;
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD width, DWORD height, DWORD bpp, DWORD refreshRate,
                                             DWORD flags) override;
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override;
    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD flags, HANDLE event) override;

private:
    ~DirectDraw7() override;
    LONG refs_ = 1;
};

// DirectDrawCreateEx of the uaRO renderer, once win::Context::open has succeeded.
HRESULT createDirectDraw(REFIID iid, void** out);

}  // namespace uagfx::dx7
