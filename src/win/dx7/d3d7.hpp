#pragma once
#include <windows.h>

#include <d3d.h>

#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

// IDirect3D7, from IDirectDraw7::QueryInterface. Its own reference count; the device does not hold it.
class Direct3D7 final : public Direct3D7Base {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE CreateDevice(REFCLSID rclsid, IDirectDrawSurface7* surface,
                                           IDirect3DDevice7** device) override;
    HRESULT STDMETHODCALLTYPE EnumZBufferFormats(REFCLSID deviceIid, LPD3DENUMPIXELFORMATSCALLBACK cb,
                                                 void* ctx) override;
    HRESULT STDMETHODCALLTYPE EvictManagedTextures() override;

private:
    LONG refs_ = 1;
};

}  // namespace uagfx::dx7
