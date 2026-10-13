#include "win/dx7/d3d7.hpp"

#include <cstring>

#include "win/context.hpp"
#include "win/dx7/device7.hpp"
#include "win/dx7/surface7.hpp"
#include "win/log.hpp"

namespace uagfx::dx7 {

HRESULT STDMETHODCALLTYPE Direct3D7::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirect3D7) {
        *out = static_cast<IDirect3D7*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirect3D7", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Direct3D7::AddRef() {
    win::Guard guard;
    return ULONG(++refs_);
}

ULONG STDMETHODCALLTYPE Direct3D7::Release() {
    win::Guard guard;
    if (refs_ <= 0) return 0;
    const LONG n = --refs_;
    if (n == 0) delete this;
    return ULONG(n);
}

HRESULT STDMETHODCALLTYPE Direct3D7::CreateDevice(REFCLSID, IDirectDrawSurface7* surface, IDirect3DDevice7** device) {
    win::Guard guard;
    if (!device) return DDERR_INVALIDPARAMS;
    *device = nullptr;
    Surface7* target = Surface7::from(surface);
    if (!target || target->role() != SurfaceRole::RenderTarget) return DDERR_INVALIDPARAMS;
    *device = new Device7(target);  // any device GUID: HAL, TnLHAL and RGB all get the same device
    const win::Context& c = win::Context::get();
    LOG_INFO("Direct3D device on the %dx%d render target", c.shadow.width(), c.shadow.height());
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Direct3D7::EnumZBufferFormats(REFCLSID, LPD3DENUMPIXELFORMATSCALLBACK cb, void* ctx) {
    if (!cb) return DDERR_INVALIDPARAMS;
    DDPIXELFORMAT pf;  // D16, which the client takes (surfaces-present-caps.md §6); the GPU depth is D32 anyway
    std::memset(&pf, 0, sizeof pf);
    pf.dwSize = sizeof pf;
    pf.dwFlags = DDPF_ZBUFFER;
    pf.dwZBufferBitDepth = 16;
    pf.dwZBitMask = 0xFFFF;
    cb(&pf, ctx);
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Direct3D7::EvictManagedTextures() { return D3D_OK; }

}  // namespace uagfx::dx7
