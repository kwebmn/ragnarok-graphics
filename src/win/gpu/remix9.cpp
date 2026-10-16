#include "win/gpu/remix9.hpp"

#include <d3d9.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/d3d7.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

using CreateFn = IDirect3D9*(WINAPI*)(UINT);

D3DMATRIX matrixOf(const float m[16]) {
    D3DMATRIX d;
    std::memcpy(&d, m, sizeof d);
    return d;
}

// D3D7's mip filter numbers are one above D3D9's (NONE 1 -> 0, POINT 2 -> 1, LINEAR 3 -> 2).
DWORD mipFilter(uint8_t f) { return f >= 1 && f <= 3 ? DWORD(f - 1) : DWORD(D3DTEXF_NONE); }

// D3D7's magnification filters go on past linear: flat and Gaussian cubic (3, 4), anisotropic (5).
DWORD magFilter(uint8_t f) {
    if (f == 5) return D3DTEXF_ANISOTROPIC;
    if (f == 3 || f == 4) return D3DTEXF_LINEAR;
    return f ? DWORD(f) : DWORD(D3DTEXF_POINT);
}

// Primitives of `count` vertices or indices; D3D7's primitive types are D3D9's.
UINT primitivesOf(uint32_t type, uint32_t count) {
    switch (type) {
        case d3d7::PT_POINTLIST: return count;
        case d3d7::PT_LINELIST: return count / 2;
        case d3d7::PT_LINESTRIP: return count >= 1 ? count - 1 : 0;
        case d3d7::PT_TRIANGLELIST: return count / 3;
        case d3d7::PT_TRIANGLESTRIP:
        case d3d7::PT_TRIANGLEFAN: return count >= 2 ? count - 2 : 0;
        default: return 0;
    }
}

D3DPRESENT_PARAMETERS paramsFor(HWND window, int width, int height, bool vsync) {
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = UINT(width);
    pp.BackBufferHeight = UINT(height);
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24X8;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    return pp;
}

// A rectangle of a texture level from the game's pixels, a row at a time.
bool writeLevel(IDirect3DTexture9* t, int level, const Rect& rect, const uint32_t* argb) {
    const RECT r{rect.left, rect.top, rect.right, rect.bottom};
    D3DLOCKED_RECT lr;
    if (FAILED(t->LockRect(UINT(level), &lr, &r, 0))) return false;
    for (int y = 0; y < rect.height(); ++y)
        std::memcpy(static_cast<uint8_t*>(lr.pBits) + size_t(y) * size_t(lr.Pitch),
                    argb + size_t(y) * size_t(rect.width()), size_t(rect.width()) * 4);
    t->UnlockRect(UINT(level));
    return true;
}

}  // namespace

Remix9::~Remix9() {
    for (auto& t : textures_) drop(t.second.texture);
    textures_.clear();
    drop(white_);
    drop(device_);
    drop(d3d_);
    // dll_ stays loaded: the game closes DirectDraw when it changes modes and opens it again, and RTX Remix's bridge
    // starts its server process when loaded - it is not made to be unloaded and loaded again in one run.
}

bool Remix9::create(const std::string& d3d9Path, HWND window, std::string& error) {
    dll_ = LoadLibraryA(d3d9Path.c_str());
    if (!dll_) {
        error = "cannot load " + d3d9Path + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    const auto create = reinterpret_cast<CreateFn>(reinterpret_cast<void*>(GetProcAddress(dll_, "Direct3DCreate9")));
    d3d_ = create ? create(D3D_SDK_VERSION) : nullptr;
    if (!d3d_) {
        error = d3d9Path + " has no working Direct3DCreate9";
        return false;
    }
    D3DADAPTER_IDENTIFIER9 id{};
    if (SUCCEEDED(d3d_->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id))) adapter_ = id.Description;
    if (!window) return true;  // the device comes with the window (setWindow, present)
    // 0.10 review 16: the device on the game's window at its size now - the game's own size comes with setTarget, a
    // Reset - so a failure here leaves the game to Direct3D 12 instead of a black screen for the whole run.
    RECT client{};
    GetClientRect(window, &client);
    window_ = window;
    width_ = client.right > 0 ? int(client.right) : 640;
    height_ = client.bottom > 0 ? int(client.bottom) : 480;
    long hr = 0;
    if (makeDevice(hr)) return true;
    char text[96];
    std::snprintf(text, sizeof text, "its Direct3D 9 device %dx%d failed: 0x%08lx", width_, height_,
                  static_cast<unsigned long>(hr));
    error = d3d9Path + ": " + text;
    return false;
}

bool Remix9::ensureDevice() {
    if (device_) return !lost_ || resetDevice();
    if (!d3d_ || !window_ || width_ <= 0 || height_ <= 0) return false;
    long hr = 0;
    if (makeDevice(hr)) return true;
    static bool logged = false;
    if (!logged) LOG_ERROR("Direct3D 9 device %dx%d failed: 0x%08lx", width_, height_, static_cast<unsigned long>(hr));
    logged = true;
    return false;
}

bool Remix9::makeDevice(long& hr) {
    D3DPRESENT_PARAMETERS pp = paramsFor(window_, width_, height_, vsync_);
    hr = d3d_->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window_,
                            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &device_);
    if (FAILED(hr)) {
        device_ = nullptr;
        return false;
    }
    if (!white_ && SUCCEEDED(device_->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &white_, nullptr))) {
        D3DLOCKED_RECT r;
        if (SUCCEEDED(white_->LockRect(0, &r, nullptr, 0))) {
            *static_cast<uint32_t*>(r.pBits) = 0xFFFFFFFF;  // D3D7 reads a missing texture as white (states.md §5)
            white_->UnlockRect(0);
        }
    }
    haveState_ = false;
    LOG_INFO("Direct3D 9 (RTX Remix mode): %dx%d on %s", width_, height_, adapter_.c_str());
    return true;
}

bool Remix9::resetDevice() {
    const HRESULT test = device_->TestCooperativeLevel();
    if (test == D3DERR_DEVICELOST) return false;  // not yet: try again later
    D3DPRESENT_PARAMETERS pp = paramsFor(window_, width_, height_, vsync_);
    if (FAILED(device_->Reset(&pp))) return false;  // managed textures survive a reset
    lost_ = false;
    haveState_ = false;
    return true;
}

bool Remix9::setTarget(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (device_ && (width != width_ || height != height_)) {
        width_ = width;
        height_ = height;
        lost_ = true;  // a new back buffer size: Reset
        return ensureDevice() || true;
    }
    width_ = width;
    height_ = height;
    ensureDevice();
    return true;  // a missing window only delays the device to the first draw or present
}

uint32_t Remix9::createTexture(int width, int height, int levels) {
    const uint32_t id = nextTexture_++;
    Texture& t = textures_[id];
    t.width = width;
    t.height = height;
    t.levels = std::max(1, levels);
    return id;
}

void Remix9::releaseTexture(uint32_t id) {
    auto it = textures_.find(id);
    if (it == textures_.end()) return;
    drop(it->second.texture);
    textures_.erase(it);
}

IDirect3DTexture9* Remix9::textureOf(uint32_t id) {
    auto it = textures_.find(id);
    return it != textures_.end() && ensureTexture(it->second) ? it->second.texture : white_;
}

// The texture, made on first use, with what the game gave for it before then.
bool Remix9::ensureTexture(Texture& t) {
    if (t.texture) return true;
    if (!device_ || FAILED(device_->CreateTexture(UINT(t.width), UINT(t.height), UINT(t.levels), 0, D3DFMT_A8R8G8B8,
                                                  D3DPOOL_MANAGED, &t.texture, nullptr))) {
        t.texture = nullptr;
        return false;
    }
    for (const Pending& p : t.pending) writeLevel(t.texture, p.level, p.rect, p.argb.data());
    std::vector<Pending>().swap(t.pending);
    return true;
}

// A texture's pixels: into the texture - a managed one takes them while the device is lost too - or, with no device
// yet, kept until it is made (0.10 review 14: the game uploads a texture once, and one lost stayed white).
void Remix9::upload(const FrameRecord& rec, const UploadCmd& u) {
    auto it = textures_.find(u.texture);
    if (it == textures_.end() || u.rect.empty()) return;
    Texture& t = it->second;
    const uint32_t* argb = rec.pixels().data() + u.pixels;
    const int level = int(u.level);
    if (ensureTexture(t)) {
        writeLevel(t.texture, level, u.rect, argb);
        return;
    }
    const int w = std::max(1, t.width >> level), h = std::max(1, t.height >> level);
    if (u.rect.left <= 0 && u.rect.top <= 0 && u.rect.right >= w && u.rect.bottom >= h)  // all of it: older is gone
        t.pending.erase(std::remove_if(t.pending.begin(), t.pending.end(),
                                       [&](const Pending& p) { return p.level == level; }),
                        t.pending.end());
    const size_t count = size_t(u.rect.width()) * size_t(u.rect.height());
    t.pending.push_back(Pending{level, u.rect, std::vector<uint32_t>(argb, argb + count)});
}

void Remix9::setCamera(bool valid, const float view[12], float) {
    cameraValid_ = valid && camera::plausibleView(view);
    if (!cameraValid_) return;
    std::memcpy(camera_, view, sizeof camera_);
    float v[16];
    camera::affine4(camera_, v);
    cameraValid_ = camera::inverse4(v, inverseView_);
    if (cameraValid_ && !haveSpace_) {
        haveSpace_ = true;
        space_ = camera::remixSpaceFor(camera_);
        LOG_INFO("remix: the game's world as Remix's: x %+.0f, y %+.0f, z %+.0f (y up, left-handed)", space_.flip[0],
                 space_.flip[1], space_.flip[2]);
    }
}

// The DrawSetup as Direct3D 9 fixed-function states; D3D7's numbers are D3D9's except some texture filters.
void Remix9::applyState(const DrawSetup& s) {
    if (haveState_ && s == state_) return;
    IDirect3DDevice9* d = device_;
    d->SetRenderState(D3DRS_LIGHTING, FALSE);
    d->SetRenderState(D3DRS_ZENABLE, s.zEnable ? D3DZB_TRUE : D3DZB_FALSE);
    d->SetRenderState(D3DRS_ZWRITEENABLE, s.zWrite ? TRUE : FALSE);
    d->SetRenderState(D3DRS_ZFUNC, s.zFunc);
    d->SetRenderState(D3DRS_CULLMODE, s.cull ? DWORD(s.cull) : DWORD(D3DCULL_NONE));
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, s.blend ? TRUE : FALSE);
    d->SetRenderState(D3DRS_SRCBLEND, s.srcBlend);
    d->SetRenderState(D3DRS_DESTBLEND, s.dstBlend);
    d->SetRenderState(D3DRS_ALPHATESTENABLE, s.alphaTest ? TRUE : FALSE);
    d->SetRenderState(D3DRS_ALPHAFUNC, s.alphaFunc);
    d->SetRenderState(D3DRS_ALPHAREF, s.alphaRef);
    d->SetRenderState(D3DRS_FOGENABLE, s.fog ? TRUE : FALSE);
    d->SetRenderState(D3DRS_FOGCOLOR, s.fogColor);
    d->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);   // vertex fog: the factor in the specular alpha
    d->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    d->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    for (DWORD i = 0; i < 2; ++i) {
        const StageSetup& st = s.stage[i];
        d->SetTextureStageState(i, D3DTSS_COLOROP, st.colorOp);
        d->SetTextureStageState(i, D3DTSS_COLORARG1, st.colorArg1);
        d->SetTextureStageState(i, D3DTSS_COLORARG2, st.colorArg2);
        d->SetTextureStageState(i, D3DTSS_ALPHAOP, st.alphaOp);
        d->SetTextureStageState(i, D3DTSS_ALPHAARG1, st.alphaArg1);
        d->SetTextureStageState(i, D3DTSS_ALPHAARG2, st.alphaArg2);
        d->SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, st.texCoord);
        d->SetSamplerState(i, D3DSAMP_ADDRESSU, st.addressU ? DWORD(st.addressU) : DWORD(D3DTADDRESS_WRAP));
        d->SetSamplerState(i, D3DSAMP_ADDRESSV, st.addressV ? DWORD(st.addressV) : DWORD(D3DTADDRESS_WRAP));
        d->SetSamplerState(i, D3DSAMP_BORDERCOLOR, st.borderColor);
        d->SetSamplerState(i, D3DSAMP_MAGFILTER, magFilter(st.magFilter));
        d->SetSamplerState(i, D3DSAMP_MINFILTER, st.minFilter ? DWORD(st.minFilter) : DWORD(D3DTEXF_POINT));
        d->SetSamplerState(i, D3DSAMP_MIPFILTER, mipFilter(st.mipFilter));
    }
    d->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
    d->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    state_ = s;
    haveState_ = true;
}

void Remix9::draw(const FrameRecord& rec, const DrawCmd& d) {
    if (d.vertexCount == 0) return;
    const uint32_t count = d.indexCount ? d.indexCount : d.vertexCount;
    const UINT primitives = primitivesOf(d.primitive, count);
    if (!primitives) return;
    const DrawSetup& s = rec.states()[d.state];
    const bool twoUv = d.fvf == d3d7::FVF_TL2;
    const UINT stride = FrameRecord::stride(d.fvf);
    const uint8_t* src = rec.vertexBytes().data() + d.vertexOffset;
    bool world = !s.ui && cameraValid_;
    if (world) {
        // x, y, z, rhw on the screen -> the point in Remix's world (ProjectVertex backwards): x y z diffuse specular
        // uv. A vertex no camera put there keeps the whole draw on the screen.
        const UINT worldStride = stride - 4;
        worldVertices_.resize(size_t(d.vertexCount) * worldStride);
        for (uint32_t v = 0; v < d.vertexCount; ++v) {
            const uint8_t* in = src + size_t(v) * stride;
            uint8_t* out = worldVertices_.data() + size_t(v) * worldStride;
            float p[4], w[3];
            std::memcpy(p, in, sizeof p);
            if (!camera::remixPoint(projection_, inverseView_, space_, p[0], p[1], p[3], w)) {
                world = false;
                break;
            }
            std::memcpy(out, w, sizeof w);
            std::memcpy(out + 12, in + 16, stride - 16);  // the colours and texture coordinates as they are
        }
        if (!world) ++flatDraws_;
    }
    // Texture coordinates as the Direct3D 12 path reads them (shaders.cpp): with one set (FVF 0x1C4) every stage
    // reads it, where Direct3D 9 reads nothing for an index past the vertex's sets; with two, stage 0 reads the
    // second only for index 1 and stage 1 the first only for index 0 (0.10 review 16).
    DrawSetup use = s;
    use.stage[0].texCoord = twoUv && s.stage[0].texCoord == 1 ? 1 : 0;
    use.stage[1].texCoord = twoUv && s.stage[1].texCoord != 0 ? 1 : 0;
    // D3D7's colour key drops the keyed texels (alpha 0 here); Direct3D 9 has no colour key, so a keyed texture
    // without an alpha test of its own gets one that drops alpha 0 - which also tells Remix it is a cut-out
    // (0.10 review 5: such draws showed black where the game shows nothing).
    if (s.colorKey && (d.keyed[0] || d.keyed[1]) && !s.alphaTest) {
        use.alphaTest = true;
        use.alphaFunc = d3d7::CMP_GREATER;
        use.alphaRef = 0;
    }
    applyState(use);
    for (DWORD i = 0; i < 2; ++i) device_->SetTexture(i, textureOf(d.texture[i]));
    const void* vertices = world ? static_cast<const void*>(worldVertices_.data()) : static_cast<const void*>(src);
    const UINT vertexStride = world ? stride - 4 : stride;
    const DWORD place = world ? D3DFVF_XYZ : D3DFVF_XYZRHW;
    device_->SetFVF(place | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | (twoUv ? D3DFVF_TEX2 : D3DFVF_TEX1));
    const D3DPRIMITIVETYPE type = D3DPRIMITIVETYPE(d.primitive);
    if (d.indexCount)
        device_->DrawIndexedPrimitiveUP(type, 0, d.vertexCount, primitives, rec.indices().data() + d.firstIndex,
                                        D3DFMT_INDEX16, vertices, vertexStride);
    else
        device_->DrawPrimitiveUP(type, primitives, vertices, vertexStride);
}

void Remix9::execute(const FrameRecord& rec) {
    if (rec.empty()) return;
    if (!ensureDevice()) {  // nothing is drawn, but the textures keep what the game gave them
        for (const Command& c : rec.commands())
            if (c.type == Cmd::Upload) upload(rec, rec.uploads()[c.index]);
        return;
    }
    projection_ = camera::projectionFor(width_, height_);
    device_->BeginScene();
    if (cameraValid_) {  // this frame's camera for every world draw: Remix sees a scene, not a picture
        float view[16], proj[16];
        camera::remixView(camera_, space_, view);
        camera::remixProjection(projection_, width_, height_, proj);
        const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const D3DMATRIX w = matrixOf(identity), v = matrixOf(view), p = matrixOf(proj);
        device_->SetTransform(D3DTS_WORLD, &w);
        device_->SetTransform(D3DTS_VIEW, &v);
        device_->SetTransform(D3DTS_PROJECTION, &p);
    }
    flatDraws_ = 0;
    for (const Command& c : rec.commands()) {
        switch (c.type) {
            case Cmd::Clear: {
                const ClearCmd& k = rec.clears()[c.index];
                const D3DRECT r{k.rect.left, k.rect.top, k.rect.right, k.rect.bottom};
                DWORD flags = 0;
                if (k.flags & 1) flags |= D3DCLEAR_TARGET;
                if (k.flags & 2) flags |= D3DCLEAR_ZBUFFER;
                device_->Clear(1, &r, flags, k.argb, k.z, 0);
                break;
            }
            case Cmd::Viewport: {
                const ViewportCmd& v = rec.viewports()[c.index];
                const D3DVIEWPORT9 vp{DWORD(std::max(0, v.rect.left)), DWORD(std::max(0, v.rect.top)),
                                      DWORD(std::max(0, v.rect.width())), DWORD(std::max(0, v.rect.height())), v.minZ,
                                      v.maxZ};
                device_->SetViewport(&vp);
                break;
            }
            case Cmd::Upload: upload(rec, rec.uploads()[c.index]); break;
            case Cmd::WriteTarget: {  // the game's own pixels into the back buffer
                const WriteTargetCmd& w = rec.writes()[c.index];
                IDirect3DSurface9* back = nullptr;
                IDirect3DSurface9* sys = nullptr;
                if (w.rect.empty() || FAILED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) break;
                if (SUCCEEDED(device_->CreateOffscreenPlainSurface(UINT(w.rect.width()), UINT(w.rect.height()),
                                                                  D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr))) {
                    D3DLOCKED_RECT lr;
                    if (SUCCEEDED(sys->LockRect(&lr, nullptr, 0))) {
                        const uint32_t* px = rec.pixels().data() + w.pixels;
                        for (int y = 0; y < w.rect.height(); ++y)
                            std::memcpy(static_cast<uint8_t*>(lr.pBits) + size_t(y) * size_t(lr.Pitch),
                                        px + size_t(y) * size_t(w.rect.width()), size_t(w.rect.width()) * 4);
                        sys->UnlockRect();
                        const POINT at{w.rect.left, w.rect.top};
                        device_->UpdateSurface(sys, nullptr, back, &at);
                    }
                    sys->Release();
                }
                back->Release();
                break;
            }
            case Cmd::Draw: draw(rec, rec.draws()[c.index]); break;
        }
    }
    device_->EndScene();
    if (flatDraws_ && !flatLogged_) {  // Remix takes them for the interface: worth knowing they exist
        flatLogged_ = true;
        LOG_INFO("remix: %u draws of the world have no camera depth: they stay on the screen", flatDraws_);
    }
}

bool Remix9::readTarget(std::vector<uint32_t>& argb) {
    if (!ensureDevice()) return false;
    IDirect3DSurface9* back = nullptr;
    IDirect3DSurface9* sys = nullptr;
    bool ok = false;
    if (SUCCEEDED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) &&
        SUCCEEDED(device_->CreateOffscreenPlainSurface(UINT(width_), UINT(height_), D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                      &sys, nullptr)) &&
        SUCCEEDED(device_->GetRenderTargetData(back, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            argb.resize(size_t(width_) * size_t(height_));
            for (int y = 0; y < height_; ++y)
                std::memcpy(argb.data() + size_t(y) * size_t(width_),
                            static_cast<const uint8_t*>(lr.pBits) + size_t(y) * size_t(lr.Pitch), size_t(width_) * 4);
            sys->UnlockRect();
            for (uint32_t& px : argb) px |= 0xFF000000u;  // X8R8G8B8: the X byte is undefined
            ok = true;
        }
    }
    drop(sys);
    drop(back);
    return ok;
}

void Remix9::present(HWND window, const PresentOptions& options) {
    if (!window_) window_ = window;  // the device's window; a later one is presented to as an override
    // The presentation interval is set with a Reset: it follows the setting, not each loading-bar Blt that shows at
    // once (0.10 review 13: a Reset every time).
    if (options.vsyncSetting != vsync_) {
        vsync_ = options.vsyncSetting;
        if (device_) lost_ = true;
    }
    if (!ensureDevice()) {
        Sleep(15);
        return;
    }
    noteShape(window ? window : window_);
    const HRESULT hr = device_->Present(nullptr, nullptr, window, nullptr);
    if (hr == D3DERR_DEVICELOST) lost_ = true;
}

// RTX Remix shows the picture over the whole window: a destination rectangle only sizes its swap chain (dxvk-remix
// D3D9SwapChainEx::UpdatePresentRegion), so there are no bars as with Direct3D 12 - a game size of another shape
// than the window is stretched (0.10 review 16). The log says so, with what keeps the picture true.
void Remix9::noteShape(HWND window) {
    RECT client{};
    if (!window || !GetClientRect(window, &client) || client.right <= 0 || client.bottom <= 0) return;
    const SIZE game{width_, height_}, shown{client.right, client.bottom};
    if (game.cx == shapeGame_.cx && game.cy == shapeGame_.cy && shown.cx == shapeWindow_.cx &&
        shown.cy == shapeWindow_.cy)
        return;
    shapeGame_ = game;
    shapeWindow_ = shown;
    const double stretch = double(game.cx) * double(shown.cy) / (double(game.cy) * double(shown.cx));
    if (std::fabs(stretch - 1.0) <= 0.01 || shapeLines_ >= 5) return;
    ++shapeLines_;
    LOG_INFO("remix: the game's %ldx%ld picture fills the %ldx%ld window, stretched %.0f%% %s: a game size of the "
             "window's shape keeps it true",
             game.cx, game.cy, shown.cx, shown.cy, std::fabs(stretch - 1.0) * 100.0,
             stretch < 1.0 ? "wider" : "taller");
}

}  // namespace uagfx::gpu
