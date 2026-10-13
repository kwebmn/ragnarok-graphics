#include "win/dx7/ddraw7.hpp"

#include <float.h>

#include <cstring>
#include <initializer_list>
#include <string>

#include "core/surface_plan.hpp"
#include "win/context.hpp"
#include "win/dx7/clipper.hpp"
#include "win/dx7/d3d7.hpp"
#include "win/dx7/formats.hpp"
#include "win/dx7/surface7.hpp"
#include "win/log.hpp"

namespace uagfx::dx7 {
namespace {

SurfaceRequest requestOf(const DDSURFACEDESC2& d) {
    SurfaceRequest r;
    r.flags = d.dwFlags;
    r.caps = d.ddsCaps.dwCaps;
    r.caps2 = d.ddsCaps.dwCaps2;
    r.width = int(d.dwWidth);
    r.height = int(d.dwHeight);
    r.backBuffers = d.dwBackBufferCount;
    r.mipLevels = d.dwMipMapCount;
    if (d.dwFlags & DDSD_PIXELFORMAT) {
        if (d.ddpfPixelFormat.dwFlags & DDPF_ZBUFFER)
            r.zBits = d.ddpfPixelFormat.dwZBufferBitDepth;
        else
            r.format = readPixelFormat(d.ddpfPixelFormat);
    }
    r.keyLow = d.ddckCKSrcBlt.dwColorSpaceLowValue;
    r.keyHigh = d.ddckCKSrcBlt.dwColorSpaceHighValue;
    return r;
}

const char* roleName(SurfaceRole r) {
    switch (r) {
        case SurfaceRole::Primary: return "primary";
        case SurfaceRole::RenderTarget: return "render target";
        case SurfaceRole::Offscreen: return "offscreen";
        case SurfaceRole::ZBuffer: return "Z-buffer";
        case SurfaceRole::Texture: return "texture";
        case SurfaceRole::Unsupported: break;
    }
    return "unsupported";
}

}  // namespace

HRESULT STDMETHODCALLTYPE DirectDraw7::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirectDraw7) {
        *out = static_cast<IDirectDraw7*>(this);
        AddRef();
        return S_OK;
    }
    if (riid == IID_IDirect3D7) {  // right after the surfaces (surfaces-present-caps.md §7, L2951)
        *out = static_cast<IDirect3D7*>(new Direct3D7);
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirectDraw7", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DirectDraw7::AddRef() {
    win::Guard guard;
    return ULONG(++refs_);
}

ULONG STDMETHODCALLTYPE DirectDraw7::Release() {
    win::Guard guard;
    if (refs_ <= 0) return 0;
    const LONG n = --refs_;
    if (n == 0) delete this;
    return ULONG(n);
}

DirectDraw7::~DirectDraw7() {
    win::Context::get().close();
    LOG_INFO("DirectDraw released, renderer closed");
}

HRESULT STDMETHODCALLTYPE DirectDraw7::CreateClipper(DWORD, LPDIRECTDRAWCLIPPER* out, IUnknown* outer) {
    if (!out) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    if (outer) return CLASS_E_NOAGGREGATION;
    *out = new Clipper;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::CreateSurface(LPDDSURFACEDESC2 desc, LPDIRECTDRAWSURFACE7* out,
                                                     IUnknown* outer) {
    win::Guard guard;
    if (!desc || !out || desc->dwSize != sizeof(DDSURFACEDESC2)) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    if (outer) return CLASS_E_NOAGGREGATION;
    win::Context& c = win::Context::get();
    const SurfacePlan plan = planSurface(requestOf(*desc), c.screenWidth, c.screenHeight, c.screenFormat);
    std::string error;
    Surface7* s = Surface7::create(plan, desc->ddsCaps.dwCaps, error);
    if (!s) {
        LOG_ERROR("CreateSurface (flags 0x%lx, caps 0x%lx, %lux%lu): %s", static_cast<unsigned long>(desc->dwFlags),
                  static_cast<unsigned long>(desc->ddsCaps.dwCaps), static_cast<unsigned long>(desc->dwWidth),
                  static_cast<unsigned long>(desc->dwHeight), error.c_str());
        return plan.role == SurfaceRole::Unsupported ? DDERR_INVALIDCAPS : DDERR_OUTOFVIDEOMEMORY;
    }
    if (plan.role == SurfaceRole::Texture)  // textures come by the thousand
        LOG_DEBUG("texture %dx%d, %d levels", plan.width, plan.height, plan.mipLevels);
    else
        LOG_INFO("surface: %s %dx%d, %d bits", roleName(plan.role), plan.width, plan.height,
                 bytesPerPixel(plan.format) * 8);
    *out = s;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::FlipToGDISurface() { return DD_OK; }  // before the client's error boxes

HRESULT STDMETHODCALLTYPE DirectDraw7::GetCaps(LPDDCAPS driverCaps, LPDDCAPS helCaps) {
    for (LPDDCAPS caps : {driverCaps, helCaps}) {  // the client reads only DDCAPS_BLTSTRETCH, and never uses it
        if (!caps) continue;
        const DWORD size = caps->dwSize;
        if (size < 2 * sizeof(DWORD) || size > sizeof(DDCAPS)) return DDERR_INVALIDPARAMS;
        std::memset(caps, 0, size);
        caps->dwSize = size;
        caps->dwCaps = DDCAPS_3D | DDCAPS_BLT | DDCAPS_BLTSTRETCH | DDCAPS_BLTCOLORFILL | DDCAPS_COLORKEY;
    }
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::GetDisplayMode(LPDDSURFACEDESC2 desc) {
    win::Guard guard;
    if (!desc || desc->dwSize != sizeof(DDSURFACEDESC2)) return DDERR_INVALIDPARAMS;
    const win::Context& c = win::Context::get();
    fillSurfaceDesc(*desc, c.screenWidth, c.screenHeight, c.screenWidth * bytesPerPixel(c.screenFormat),
                    c.screenFormat, 0, nullptr);
    desc->dwFlags &= ~DWORD(DDSD_CAPS);
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::RestoreAllSurfaces() { return DD_OK; }  // nothing is ever lost

HRESULT STDMETHODCALLTYPE DirectDraw7::RestoreDisplayMode() { return DD_OK; }

HRESULT STDMETHODCALLTYPE DirectDraw7::SetCooperativeLevel(HWND hwnd, DWORD flags) {
    win::Guard guard;
    win::Context& c = win::Context::get();
    if (hwnd) c.window = hwnd;
    if (flags & DDSCL_FPUSETUP) _control87(_PC_24, _MCW_PC);  // the x87 state DirectX 7 leaves behind
    LOG_INFO("SetCooperativeLevel(0x%lx)%s", static_cast<unsigned long>(flags),
             (flags & DDSCL_FULLSCREEN) ? ": full screen becomes a borderless window" : "");
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::SetDisplayMode(DWORD width, DWORD height, DWORD bpp, DWORD, DWORD) {
    win::Guard guard;
    if (width == 0 || height == 0 || (bpp != 16 && bpp != 32)) return DDERR_INVALIDMODE;
    win::Context& c = win::Context::get();
    c.screenWidth = int(width);
    c.screenHeight = int(height);
    c.screenFormat = bpp == 16 ? PixelFormat::R5G6B5 : PixelFormat::X8R8G8B8;
    LOG_INFO("display mode %lux%lu, %lu bits: the game's size; the monitor keeps its own mode",
             static_cast<unsigned long>(width), static_cast<unsigned long>(height), static_cast<unsigned long>(bpp));
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::TestCooperativeLevel() { return DD_OK; }

HRESULT STDMETHODCALLTYPE DirectDraw7::WaitForVerticalBlank(DWORD, HANDLE) {
    win::Guard guard;
    win::Context::get().vblankWaited = true;  // the Blt that follows presents with vsync instead of waiting here
    return DD_OK;
}

HRESULT createDirectDraw(REFIID iid, void** out) {
    if (!out) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    if (iid != IID_IDirectDraw7) return DDERR_INVALIDPARAMS;  // what DirectDrawCreateEx itself accepts
    *out = static_cast<IDirectDraw7*>(new DirectDraw7);
    return DD_OK;
}

}  // namespace uagfx::dx7
