#pragma once
#include <windows.h>

#include <ddraw.h>

#include <memory>
#include <string>

#include "core/blit.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/surface_ops.hpp"
#include "core/surface_plan.hpp"
#include "core/texture_levels.hpp"
#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

// IDirectDrawSurface7. Where the pixels live depends on the role (core/surface_plan.hpp):
// - Offscreen: an Image in CPU memory (the screen copy, the calibration readback, big pictures);
// - Texture: one level of TextureLevels, owned by the top level of the mip chain and sent to its GPU texture
//   before a draw samples it;
// - RenderTarget: the GPU target; Lock and GetDC go through the context's CPU copy (win::Shadow);
// - Primary: the window (Flip and Blt onto it show a frame); ZBuffer: the renderer's depth, no pixels.
// Surfaces created together with a top surface (the back buffer of a flip chain, mip levels) live and die with it
// and share its reference count, as in DirectDraw.
class Surface7 final : public Surface7Base {
public:
    static constexpr uint32_t kMagic = 0x37534655;  // "UFS7": tells our surfaces from anything else passed in

    // A new surface with one reference, or nullptr with the reason in error.
    static Surface7* create(const SurfacePlan& plan, DWORD caps, std::string& error);
    // A surface the client hands back to us; nullptr when it is not one of ours.
    static Surface7* from(IDirectDrawSurface7* s);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE7 surface) override;
    HRESULT STDMETHODCALLTYPE Blt(LPRECT dstRect, LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect, DWORD flags,
                                  LPDDBLTFX fx) override;
    HRESULT STDMETHODCALLTYPE BltFast(DWORD x, DWORD y, LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect,
                                      DWORD flags) override;
    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD flags, LPDIRECTDRAWSURFACE7 surface) override;
    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE7 target, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS2 caps, LPDIRECTDRAWSURFACE7* out) override;
    HRESULT STDMETHODCALLTYPE GetCaps(LPDDSCAPS2 caps) override;
    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD flags, LPDDCOLORKEY key) override;
    HRESULT STDMETHODCALLTYPE GetDC(HDC* out) override;
    HRESULT STDMETHODCALLTYPE GetPixelFormat(LPDDPIXELFORMAT format) override;
    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(LPDDSURFACEDESC2 desc) override;
    HRESULT STDMETHODCALLTYPE IsLost() override;
    HRESULT STDMETHODCALLTYPE Lock(LPRECT rect, LPDDSURFACEDESC2 desc, DWORD flags, HANDLE event) override;
    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC dc) override;
    HRESULT STDMETHODCALLTYPE Restore() override;
    HRESULT STDMETHODCALLTYPE SetClipper(LPDIRECTDRAWCLIPPER clipper) override;
    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD flags, LPDDCOLORKEY key) override;
    HRESULT STDMETHODCALLTYPE Unlock(LPRECT rect) override;

    SurfaceRole role() const { return role_; }
    Surface7* zBuffer() const { return zBuffer_; }
    // Textures: the GPU texture of the chain, whether it has a colour key, and the uploads a draw needs first.
    uint32_t gpuTexture() const { return top()->gpuTexture_; }
    bool colorKeyed() const { return colorKey().enabled; }
    void sync(FrameRecord& record);

private:
    Surface7(SurfaceRole role, DWORD caps, int width, int height, PixelFormat format);
    ~Surface7() override;
    Surface7* top() { return root_ ? root_ : this; }
    const Surface7* top() const { return root_ ? root_ : this; }
    Rect bounds() const { return Rect{0, 0, width_, height_}; }
    Place place() const;
    Image& pixels();  // Offscreen and Texture only
    ColorKey colorKey() const;
    int pitch() const;
    void changed(const Rect& r);
    void describe(DDSURFACEDESC2& d, void* bits) const;
    HRESULT blit(BlitAction action, const Rect& dr, Surface7* src, const Rect& sr, const DDBLTFX* fx,
                 const ColorKey& key);
    void freeDc();

    uint32_t magic_ = kMagic;
    LONG refs_ = 1;
    SurfaceRole role_;
    DWORD caps_;
    int width_;
    int height_;
    PixelFormat format_;
    Image image_;                            // Offscreen
    ColorKey key_;                           // Offscreen; a texture keeps its key in TextureLevels
    std::unique_ptr<TextureLevels> levels_;  // Texture, top level only
    int level_ = 0;
    uint32_t gpuTexture_ = 0;                // Texture, top level only
    Surface7* root_ = nullptr;               // the top surface this one was created with
    Surface7* next_ = nullptr;               // owned: the back buffer of a primary, the next mip level
    Surface7* zBuffer_ = nullptr;            // counted: AddAttachedSurface
    IDirectDrawClipper* clipper_ = nullptr;  // counted
    bool locked_ = false;
    Rect lockRect_;
    HDC dc_ = nullptr;
    HBITMAP dib_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    uint32_t* dibBits_ = nullptr;
    std::vector<uint32_t> dcBefore_;  // the frame as GetDC gave it: ReleaseDC writes back only what GDI changed
};

}  // namespace uagfx::dx7
