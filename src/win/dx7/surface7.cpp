#include "win/dx7/surface7.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "core/d3d7.hpp"
#include "core/full_colour.hpp"
#include "win/context.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dx7/formats.hpp"
#include "win/log.hpp"
#include "win/sprite_hook.hpp"

namespace uagfx::dx7 {
namespace {

Rect toRect(const RECT& r) { return Rect{int(r.left), int(r.top), int(r.right), int(r.bottom)}; }

bool within(const Rect& r, const Rect& bounds) {
    return !r.empty() && r.left >= bounds.left && r.top >= bounds.top && r.right <= bounds.right &&
           r.bottom <= bounds.bottom;
}

// DDSURFACEDESC2, or the older DDSURFACEDESC a library like Bink may pass: it is the first 108 bytes of the new one.
bool descSizeOk(DWORD size) { return size == sizeof(DDSURFACEDESC2) || size == sizeof(DDSURFACEDESC); }

void copyDesc(LPDDSURFACEDESC2 out, const DDSURFACEDESC2& d) {
    const DWORD size = out->dwSize;
    std::memcpy(out, &d, size);
    out->dwSize = size;
}

void zPixelFormat(DDPIXELFORMAT& pf) {
    std::memset(&pf, 0, sizeof pf);
    pf.dwSize = sizeof pf;
    pf.dwFlags = DDPF_ZBUFFER;
    pf.dwZBufferBitDepth = 16;
    pf.dwZBitMask = 0xFFFF;
}

}  // namespace

Surface7::Surface7(SurfaceRole role, DWORD caps, int width, int height, PixelFormat format)
    : role_(role), caps_(caps), width_(width), height_(height), format_(format) {}

Surface7::~Surface7() {
    magic_ = 0;
    win::crisp::forgetSurface(this);  // stage 6: a tile's surface is gone
    win::Context& c = win::Context::get();
    if (locked_ && role_ == SurfaceRole::RenderTarget) c.shadow.unlock();
    freeDc();
    delete next_;
    if (zBuffer_) zBuffer_->Release();
    if (clipper_) clipper_->Release();
    if (gpuTexture_ && c.renderer) c.renderer->releaseTexture(gpuTexture_);
}

Surface7* Surface7::create(const SurfacePlan& plan, DWORD caps, std::string& error) {
    if (plan.role == SurfaceRole::Unsupported) {
        error = plan.error;
        return nullptr;
    }
    win::Context& c = win::Context::get();
    Surface7* s = new Surface7(plan.role, caps, plan.width, plan.height, plan.format);
    // A key range written the wrong way round means the single colour in low.
    const ColorKey key{plan.colorKey, plan.keyLow, std::max(plan.keyLow, plan.keyHigh)};
    switch (plan.role) {
        case SurfaceRole::Offscreen:
            s->image_ = Image(plan.width, plan.height, plan.format);
            s->key_ = key;
            break;
        case SurfaceRole::Texture: {
            s->levels_.reset(new TextureLevels(plan.width, plan.height, plan.format, plan.mipLevels));
            if (key.enabled) s->levels_->setKey(key);
            s->gpuTexture_ = c.renderer ? c.renderer->createTexture(plan.width, plan.height, s->levels_->levels()) : 0;
            if (!s->gpuTexture_) {
                error = "the GPU texture could not be created";
                delete s;
                return nullptr;
            }
            Surface7* last = s;
            for (int i = 1; i < s->levels_->levels(); ++i) {
                const Image& img = s->levels_->level(i);
                Surface7* m = new Surface7(SurfaceRole::Texture, caps & ~DWORD(ddcaps::COMPLEX), img.width(),
                                           img.height(), plan.format);
                m->root_ = s;
                m->level_ = i;
                last->next_ = m;
                last = m;
            }
            break;
        }
        case SurfaceRole::RenderTarget:
            if (!c.setTarget(plan.width, plan.height)) {
                error = "the render target could not be created";
                delete s;
                return nullptr;
            }
            break;
        case SurfaceRole::Primary:
            if (plan.backBuffer) {
                if (!c.setTarget(plan.width, plan.height)) {
                    error = "the render target could not be created";
                    delete s;
                    return nullptr;
                }
                Surface7* back = new Surface7(SurfaceRole::RenderTarget,
                                              ddcaps::BACKBUFFER | ddcaps::D3DDEVICE | ddcaps::FLIP | ddcaps::COMPLEX |
                                                  ddcaps::VIDEOMEMORY,
                                              plan.width, plan.height, plan.format);
                back->root_ = s;
                s->next_ = back;
            }
            break;
        case SurfaceRole::ZBuffer:
        case SurfaceRole::Unsupported:
            break;
    }
    return s;
}

uint8_t Surface7::spriteAt(float x, float y, bool* card) const {
    for (const Cell& s : top()->cells_)
        if (x >= float(s.rect.left) && x < float(s.rect.right) && y >= float(s.rect.top) && y < float(s.rect.bottom)) {
            if (card) *card = s.card;
            return s.source;
        }
    if (card) *card = false;
    return 0;
}

Surface7* Surface7::from(IDirectDrawSurface7* s) {
    Surface7* p = static_cast<Surface7*>(s);
    return p && p->magic_ == kMagic ? p : nullptr;
}

bool Surface7::fullColour(int x, int y, int w, int h, const uint32_t* picture, int pitch) {
    win::Guard guard;
    if (role_ != SurfaceRole::Texture || level_ != 0 || !picture || pitch <= 0) return false;
    TextureLevels& levels = *top()->levels_;
    const Image& top0 = levels.level(0);
    if (!full_colour::shortFormat(top0.format())) return false;
    const full_colour::Write at = full_colour::writeOf(x, y, w, h, top0.width(), top0.height());
    if (at.rect.empty()) return false;
    try {  // called from the game's own code: nothing may be thrown back into it
        std::vector<uint32_t> texels;
        full_colour::texelsOf(picture, pitch, at, texels);
        return levels.fullColour(at.rect, texels.data(), true);  // the game made its mip levels again too
    } catch (...) {  // no memory for it (the game's address space): the 16-bit texels
        return false;
    }
}

HRESULT STDMETHODCALLTYPE Surface7::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirectDrawSurface7) {
        *out = static_cast<IDirectDrawSurface7*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirectDrawSurface7", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Surface7::AddRef() {
    win::Guard guard;
    return ULONG(++top()->refs_);
}

ULONG STDMETHODCALLTYPE Surface7::Release() {
    win::Guard guard;
    Surface7* t = top();
    if (t->refs_ <= 0) return 0;  // more Releases than references: ignored rather than freed twice
    const LONG n = --t->refs_;
    if (n == 0) delete t;  // a chain goes as a whole
    return ULONG(n);
}

Place Surface7::place() const {
    if (role_ == SurfaceRole::RenderTarget) return Place::Target;
    if (role_ == SurfaceRole::Primary) return Place::Front;
    return Place::Cpu;
}

Image& Surface7::pixels() { return role_ == SurfaceRole::Texture ? top()->levels_->level(level_) : image_; }

ColorKey Surface7::colorKey() const { return role_ == SurfaceRole::Texture ? top()->levels_->key() : key_; }

int Surface7::pitch() const {
    switch (role_) {
        case SurfaceRole::Offscreen: return image_.pitch();
        case SurfaceRole::Texture: return top()->levels_->level(level_).pitch();
        case SurfaceRole::RenderTarget: return win::Context::get().shadow.pitch();
        case SurfaceRole::ZBuffer: return width_ * 2;
        default: return width_ * bytesPerPixel(format_);
    }
}

void Surface7::changed(const Rect& r) {
    if (role_ == SurfaceRole::Texture) top()->levels_->changed(level_, r);
}

void Surface7::sync(FrameRecord& record) {
    if (role_ == SurfaceRole::Texture) top()->levels_->sync(record, top()->gpuTexture_);
}

void Surface7::describe(DDSURFACEDESC2& d, void* bits) const {
    d.dwSize = sizeof d;
    fillSurfaceDesc(d, width_, height_, pitch(), format_, caps_, bits);
    if (role_ == SurfaceRole::ZBuffer) zPixelFormat(d.ddpfPixelFormat);
    const ColorKey k = colorKey();
    if (k.enabled) {
        d.dwFlags |= DDSD_CKSRCBLT;
        d.ddckCKSrcBlt.dwColorSpaceLowValue = k.low;
        d.ddckCKSrcBlt.dwColorSpaceHighValue = k.high;
    }
    if (role_ == SurfaceRole::Texture && top()->levels_->levels() > 1) {
        d.dwFlags |= DDSD_MIPMAPCOUNT;
        d.dwMipMapCount = DWORD(top()->levels_->levels() - level_);
    }
}

HRESULT STDMETHODCALLTYPE Surface7::AddAttachedSurface(LPDIRECTDRAWSURFACE7 surface) {
    win::Guard guard;
    Surface7* z = from(surface);
    if (!z || z->role_ != SurfaceRole::ZBuffer || role_ != SurfaceRole::RenderTarget) return DDERR_CANNOTATTACHSURFACE;
    if (z == zBuffer_) return DDERR_SURFACEALREADYATTACHED;
    z->AddRef();
    if (zBuffer_) zBuffer_->Release();
    zBuffer_ = z;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::DeleteAttachedSurface(DWORD, LPDIRECTDRAWSURFACE7 surface) {
    win::Guard guard;
    if (!zBuffer_ || (surface && from(surface) != zBuffer_)) return DDERR_SURFACENOTATTACHED;
    Surface7* z = zBuffer_;
    zBuffer_ = nullptr;
    z->Release();
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::Blt(LPRECT dstRect, LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect, DWORD flags,
                                       LPDDBLTFX fx) {
    win::Guard guard;
    Surface7* src = from(srcSurface);
    if (srcSurface && !src) return DDERR_INVALIDPARAMS;
    if (role_ == SurfaceRole::ZBuffer || (src && src->role_ == SurfaceRole::ZBuffer))
        return notImplemented("IDirectDrawSurface7::Blt (Z-buffer)");
    BlitRequest q;
    q.dst = place();
    q.src = src ? src->place() : Place::Cpu;
    q.hasSource = src != nullptr;
    q.flags = flags;
    const Rect dr = dstRect ? toRect(*dstRect) : bounds();
    const Rect sr = src ? (srcRect ? toRect(*srcRect) : src->bounds()) : Rect{};
    const ColorKey key = src && (flags & ddblt::KEYSRC) ? src->colorKey() : ColorKey{};
    return blit(routeBlit(q), dr, src, sr, fx, key);
}

HRESULT STDMETHODCALLTYPE Surface7::BltFast(DWORD x, DWORD y, LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect,
                                           DWORD flags) {
    win::Guard guard;
    Surface7* src = from(srcSurface);
    if (!src) return DDERR_INVALIDPARAMS;
    if (role_ == SurfaceRole::ZBuffer || src->role_ == SurfaceRole::ZBuffer)
        return notImplemented("IDirectDrawSurface7::BltFast (Z-buffer)");
    BlitRequest q;
    q.dst = place();
    q.src = src->place();
    q.hasSource = true;
    q.flags = ddblt::WAIT;
    const Rect sr = srcRect ? toRect(*srcRect) : src->bounds();
    const Rect dr{int(x), int(y), int(x) + sr.width(), int(y) + sr.height()};
    const ColorKey key = (flags & ddbltfast::SRCCOLORKEY) ? src->colorKey() : ColorKey{};
    return blit(routeBlit(q), dr, src, sr, nullptr, key);
}

HRESULT Surface7::blit(BlitAction action, const Rect& dr, Surface7* src, const Rect& sr, const DDBLTFX* fx,
                       const ColorKey& key) {
    win::Context& c = win::Context::get();
    const Rect target{0, 0, c.shadow.width(), c.shadow.height()};
    switch (action) {
        case BlitAction::Fill: {  // fx->dwFillColor is already in this surface's format
            if (!fx) return DDERR_INVALIDPARAMS;
            Rect r = dr;
            if (clipRect(bounds(), r)) {
                fill(pixels(), r, fx->dwFillColor);
                changed(r);
            }
            return DD_OK;
        }
        case BlitAction::FillTarget: {
            if (!fx) return DDERR_INVALIDPARAMS;
            ClearCmd clear;
            clear.flags = d3d7::CLEAR_TARGET;
            clear.argb = toArgb8(format_, fx->dwFillColor);
            clear.rect = dr;
            if (clipRect(target, clear.rect)) c.record.clear(clear);
            return DD_OK;
        }
        case BlitAction::CopyCpu:
            if (!blitCpu(pixels(), dr, src->pixels(), sr, key)) return DDERR_INVALIDRECT;
            changed(dr);
            return DD_OK;
        case BlitAction::ReadTarget: {  // calibration readback, the fade's saved frame
            std::vector<uint32_t> argb;
            if (!c.readTarget(argb)) return DDERR_GENERIC;
            if (!readTargetInto(pixels(), dr, argb, target.width(), target.height(), sr)) return DDERR_INVALIDRECT;
            changed(dr);
            return DD_OK;
        }
        case BlitAction::WriteTarget: {  // the fade's restored frame, pictures bigger than a texture
            std::vector<uint32_t> current;
            if (key.enabled && !c.readTarget(current)) return DDERR_GENERIC;
            TargetWrite w;
            if (!targetWrite(src->pixels(), sr, dr, target.width(), target.height(), key, current, w))
                return DDERR_INVALIDRECT;
            if (!w.rect.empty()) c.writeTarget(w.rect, w.argb.data());
            return DD_OK;
        }
        case BlitAction::Present: {  // windowed present, the loading bar, Bink's 512x347 frame
            Rect s = sr;
            const bool vblank = c.vblankWaited;  // only the windowed game frame waits for it first
            c.vblankWaited = false;
            if (clipRect(target, s)) c.present(s, vblank);
            return DD_OK;
        }
        case BlitAction::Unsupported:
            break;
    }
    return notImplemented("IDirectDrawSurface7::Blt (this pair of surfaces)");
}

HRESULT STDMETHODCALLTYPE Surface7::Flip(LPDIRECTDRAWSURFACE7, DWORD) {
    win::Guard guard;
    if (role_ != SurfaceRole::Primary || !next_) return DDERR_NOTFLIPPABLE;
    win::Context& c = win::Context::get();
    c.present(Rect{0, 0, c.shadow.width(), c.shadow.height()}, true);  // a copy: the back buffer keeps its pixels
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetAttachedSurface(LPDDSCAPS2 caps, LPDIRECTDRAWSURFACE7* out) {
    win::Guard guard;
    if (!caps || !out) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    Surface7* found = nullptr;
    if (caps->dwCaps & ddcaps::ZBUFFER)
        found = zBuffer_;
    else if (role_ == SurfaceRole::Primary && (caps->dwCaps & (ddcaps::BACKBUFFER | ddcaps::FLIP)))
        found = next_;
    else if (role_ == SurfaceRole::Texture && (caps->dwCaps & (ddcaps::MIPMAP | ddcaps::TEXTURE)))
        found = next_;
    if (!found) return DDERR_NOTFOUND;
    found->AddRef();
    *out = found;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetCaps(LPDDSCAPS2 caps) {
    if (!caps) return DDERR_INVALIDPARAMS;
    std::memset(caps, 0, sizeof *caps);
    caps->dwCaps = caps_;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetColorKey(DWORD flags, LPDDCOLORKEY key) {
    win::Guard guard;
    if (!key || !(flags & DDCKEY_SRCBLT)) return DDERR_INVALIDPARAMS;
    const ColorKey k = colorKey();
    if (!k.enabled) return DDERR_NOCOLORKEY;
    key->dwColorSpaceLowValue = k.low;
    key->dwColorSpaceHighValue = k.high;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::SetColorKey(DWORD flags, LPDDCOLORKEY key) {
    win::Guard guard;
    if (!(flags & DDCKEY_SRCBLT)) return notImplemented("IDirectDrawSurface7::SetColorKey (not a source key)");
    ColorKey k;
    if (key) {
        k.enabled = true;
        k.low = key->dwColorSpaceLowValue;
        k.high = (flags & DDCKEY_COLORSPACE) ? std::max(key->dwColorSpaceLowValue, key->dwColorSpaceHighValue)
                                             : key->dwColorSpaceLowValue;
    }
    if (role_ == SurfaceRole::Texture)
        top()->levels_->setKey(k);
    else
        key_ = k;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetDC(HDC* out) {
    win::Guard guard;
    if (!out) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    if (role_ != SurfaceRole::RenderTarget) return notImplemented("IDirectDrawSurface7::GetDC (not the render target)");
    if (dc_) return DDERR_DCALREADYCREATED;
    // The screenshot caption is drawn over the current frame (surfaces-present-caps.md §8).
    win::Context& c = win::Context::get();
    std::vector<uint32_t> argb;
    if (!c.readTarget(argb)) return DDERR_GENERIC;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = width_;
    bi.bmiHeader.biHeight = -height_;  // top-down, like the target
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    dc_ = CreateCompatibleDC(nullptr);
    if (dc_) dib_ = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib_ || !bits) {
        freeDc();
        return DDERR_GENERIC;
    }
    dibBits_ = static_cast<uint32_t*>(bits);
    std::memcpy(dibBits_, argb.data(), std::min(argb.size(), size_t(width_) * size_t(height_)) * sizeof(uint32_t));
    dcBefore_.assign(dibBits_, dibBits_ + size_t(width_) * size_t(height_));
    oldBitmap_ = SelectObject(dc_, dib_);  // ARGB8 in memory is the DIB's B, G, R, X
    *out = dc_;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::ReleaseDC(HDC dc) {
    win::Guard guard;
    if (!dc_ || dc != dc_) return DDERR_NODC;
    GdiFlush();
    // Only what GDI drew goes back (review I1): at k > 1 the whole frame would be the shrunk copy stretched over the
    // sharp picture, and the screenshot read after it would be resampled twice.
    const Rect r = changedRect(dcBefore_.data(), dibBits_, width_, height_);
    if (!r.empty()) {
        std::vector<uint32_t> argb(size_t(r.width()) * size_t(r.height()));
        for (int y = r.top; y < r.bottom; ++y)
            for (int x = r.left; x < r.right; ++x)  // GDI leaves the X byte 0
                argb[size_t(y - r.top) * size_t(r.width()) + size_t(x - r.left)] =
                    dibBits_[size_t(y) * size_t(width_) + size_t(x)] | 0xFF000000u;
        win::Context::get().writeTarget(r, argb.data());
    }
    freeDc();
    return DD_OK;
}

void Surface7::freeDc() {
    if (dc_ && oldBitmap_) SelectObject(dc_, oldBitmap_);
    if (dib_) DeleteObject(dib_);
    if (dc_) DeleteDC(dc_);
    dc_ = nullptr;
    dib_ = nullptr;
    oldBitmap_ = nullptr;
    dibBits_ = nullptr;
    dcBefore_.clear();
}

HRESULT STDMETHODCALLTYPE Surface7::GetPixelFormat(LPDDPIXELFORMAT format) {
    win::Guard guard;
    if (!format) return DDERR_INVALIDPARAMS;
    if (role_ == SurfaceRole::ZBuffer)
        zPixelFormat(*format);
    else
        fillPixelFormat(*format, format_);
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetSurfaceDesc(LPDDSURFACEDESC2 desc) {
    win::Guard guard;
    if (!desc || !descSizeOk(desc->dwSize)) return DDERR_INVALIDPARAMS;
    DDSURFACEDESC2 d{};
    describe(d, nullptr);
    copyDesc(desc, d);
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::IsLost() { return DD_OK; }  // never lost: everything lives in our memory

HRESULT STDMETHODCALLTYPE Surface7::Restore() { return DD_OK; }

HRESULT STDMETHODCALLTYPE Surface7::Lock(LPRECT rect, LPDDSURFACEDESC2 desc, DWORD, HANDLE) {
    win::Guard guard;
    if (!desc || !descSizeOk(desc->dwSize)) return DDERR_INVALIDPARAMS;
    if (locked_) return DDERR_SURFACEBUSY;
    const Rect r = rect ? toRect(*rect) : bounds();
    if (!within(r, bounds())) return DDERR_INVALIDRECT;
    uint8_t* bits = nullptr;
    switch (role_) {
        case SurfaceRole::Offscreen:
        case SurfaceRole::Texture:
            bits = pixelAddress(pixels(), r.left, r.top);
            break;
        case SurfaceRole::RenderTarget: {  // every frame, mostly without touching the memory (Review Focus 1)
            win::Shadow& sh = win::Context::get().shadow;
            uint8_t* mem = sh.lock();
            if (!mem) {
                sh.unlock();
                return DDERR_GENERIC;
            }
            bits = mem + size_t(r.top) * size_t(sh.pitch()) + size_t(r.left) * size_t(bytesPerPixel(sh.format()));
            break;
        }
        default:
            return notImplemented("IDirectDrawSurface7::Lock (primary or Z-buffer)");
    }
    locked_ = true;
    lockRect_ = r;
    DDSURFACEDESC2 d{};
    describe(d, bits);
    copyDesc(desc, d);
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::Unlock(LPRECT) {
    win::Guard guard;
    if (!locked_) return DDERR_NOTLOCKED;  // the client unlocks everything before an error box
    locked_ = false;
    if (role_ == SurfaceRole::Texture) {
        changed(lockRect_);
        Rect cell;  // AI textures: a frame the game wrote into a cell of its sprite cache (sprite_hook.hpp)
        uint8_t source = 0;
        if (level_ == 0 && win::spriteCell(cell, source)) {
            // 2.5.23: a frame in a dye - the same frame in its sprite's own colours, made 32-bit as its texels are
            std::vector<uint32_t> own;
            const std::vector<uint16_t>* own16 = win::spriteCellOwn();
            const TextureLevels& lv = *top()->levels_;
            const PixelFormat f = lv.level(0).format();
            if (own16 && bytesPerPixel(f) == 2 && own16->size() == size_t(cell.width()) * size_t(cell.height())) {
                own.resize(own16->size());
                const auto* bytes = reinterpret_cast<const uint8_t*>(own16->data());
                if (lv.key().enabled)
                    rowToArgb8Keyed(f, bytes, own.data(), int(own.size()), lv.key().low, lv.key().high);
                else
                    rowToArgb8(f, bytes, own.data(), int(own.size()));
            }
            top()->levels_->slotWritten(cell, source, own.empty() ? nullptr : &own);
            std::vector<Cell>& cells = top()->cells_;  // 0.93
            const bool card = win::spriteCellCard();
            bool known = false;
            for (Cell& s : cells)
                if (s.rect == cell) {
                    s.source = source;
                    s.card = card;
                    known = true;
                }
            if (!known && cells.size() < 1024) cells.push_back(Cell{cell, source, card});
        }
    }
    if (role_ == SurfaceRole::RenderTarget) {
        win::Context& c = win::Context::get();
        if (c.shadow.unlock()) {  // written (the wave effect, Bink): the GPU target gets the memory
            std::vector<uint32_t> argb;
            c.shadow.toArgb(argb);
            c.writeTarget(Rect{0, 0, c.shadow.width(), c.shadow.height()}, argb.data());
        }
    }
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::SetClipper(LPDIRECTDRAWCLIPPER clipper) {
    win::Guard guard;
    if (clipper) clipper->AddRef();
    if (clipper_) clipper_->Release();
    clipper_ = clipper;
    return DD_OK;
}

}  // namespace uagfx::dx7
