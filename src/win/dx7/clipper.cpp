#include "win/dx7/clipper.hpp"

#include "win/context.hpp"

namespace uagfx::dx7 {

HRESULT STDMETHODCALLTYPE Clipper::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirectDrawClipper) {
        *out = static_cast<IDirectDrawClipper*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirectDrawClipper", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Clipper::AddRef() {
    win::Guard guard;
    return ULONG(++refs_);
}

ULONG STDMETHODCALLTYPE Clipper::Release() {
    win::Guard guard;
    if (refs_ <= 0) return 0;
    const LONG n = --refs_;
    if (n == 0) delete this;
    return ULONG(n);
}

HRESULT STDMETHODCALLTYPE Clipper::GetHWnd(HWND* out) {
    win::Guard guard;
    if (!out) return DDERR_INVALIDPARAMS;
    *out = hwnd_;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Clipper::SetHWnd(DWORD, HWND hwnd) {
    win::Guard guard;
    hwnd_ = hwnd;
    return DD_OK;
}

}  // namespace uagfx::dx7
