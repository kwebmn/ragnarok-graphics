#include "win/dx7/device7.hpp"

#include <algorithm>
#include <cstring>

#include "core/device_caps.hpp"
#include "core/frame.hpp"
#include "core/glint.hpp"
#include "core/sprite_exe.hpp"
#include "win/context.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dx7/caps.hpp"
#include "win/dx7/formats.hpp"
#include "win/dx7/surface7.hpp"
#include "win/effect_hook.hpp"
#include "win/log.hpp"

namespace uagfx::dx7 {

Device7::Device7(Surface7* target) : target_(target) {
    target_->AddRef();
    const win::Context& c = win::Context::get();  // D3D7's default viewport: the whole target
    viewport_.dwWidth = DWORD(c.shadow.width());
    viewport_.dwHeight = DWORD(c.shadow.height());
    viewport_.dvMaxZ = 1.0f;
}

Device7::~Device7() {
    for (Surface7*& t : textures_)
        if (t) t->Release();
    target_->Release();
}

HRESULT STDMETHODCALLTYPE Device7::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirect3DDevice7) {
        *out = static_cast<IDirect3DDevice7*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirect3DDevice7", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Device7::AddRef() {
    win::Guard guard;
    return ULONG(++refs_);
}

ULONG STDMETHODCALLTYPE Device7::Release() {
    win::Guard guard;
    if (refs_ <= 0) return 0;
    const LONG n = --refs_;
    if (n == 0) delete this;
    return ULONG(n);
}

HRESULT STDMETHODCALLTYPE Device7::GetCaps(D3DDEVICEDESC7* desc) {
    if (!desc) return DDERR_INVALIDPARAMS;
    fillDeviceDesc(*desc);
    return D3D_OK;  // exactly 0: otherwise the client keeps a max texture size of 0 (surfaces-present-caps.md §5)
}

HRESULT STDMETHODCALLTYPE Device7::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void* ctx) {
    if (!cb) return DDERR_INVALIDPARAMS;
    for (PixelFormat f : textureFormats()) {
        DDPIXELFORMAT pf;
        fillPixelFormat(pf, f);
        if (cb(&pf, ctx) == D3DENUMRET_CANCEL) break;  // the client stops at its match
    }
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::BeginScene() {
    win::Guard guard;
    scene_.beginScene();
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::EndScene() {
    win::Guard guard;
    win::Context::get().flush();  // the GPU starts on the frame while the client gets ready to present it
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetRenderTarget(IDirectDrawSurface7* surface, DWORD) {
    win::Guard guard;
    Surface7* t = Surface7::from(surface);
    if (!t || t->role() != SurfaceRole::RenderTarget) return DDERR_INVALIDPARAMS;
    t->AddRef();  // there is one GPU target; its depth stands for the attached Z-buffer
    target_->Release();
    target_ = t;
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetRenderTarget(IDirectDrawSurface7** surface) {
    win::Guard guard;
    if (!surface) return DDERR_INVALIDPARAMS;
    target_->AddRef();
    *surface = target_;
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::Clear(DWORD count, D3DRECT* rects, DWORD flags, D3DCOLOR color, D3DVALUE z, DWORD) {
    win::Guard guard;
    win::Context& c = win::Context::get();
    ClearCmd cmd;
    cmd.flags = flags & (d3d7::CLEAR_TARGET | d3d7::CLEAR_ZBUFFER);  // no stencil
    cmd.argb = color;
    cmd.z = z;
    if (!cmd.flags) return D3D_OK;
    const Rect vp{int(viewport_.dwX), int(viewport_.dwY), int(viewport_.dwX + viewport_.dwWidth),
                  int(viewport_.dwY + viewport_.dwHeight)};
    if (count == 0 || !rects) {  // the whole viewport (the calibration clear)
        cmd.rect = vp;
        if (clipRect(Rect{0, 0, c.shadow.width(), c.shadow.height()}, cmd.rect)) c.record.clear(cmd);
        return D3D_OK;
    }
    for (DWORD i = 0; i < count; ++i) {
        cmd.rect = Rect{int(rects[i].x1), int(rects[i].y1), int(rects[i].x2), int(rects[i].y2)};
        if (clipRect(vp, cmd.rect)) c.record.clear(cmd);
    }
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetTransform(D3DTRANSFORMSTATETYPE, D3DMATRIX*) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::SetViewport(D3DVIEWPORT7* data) {
    win::Guard guard;
    if (!data) return DDERR_INVALIDPARAMS;
    viewport_ = *data;
    ViewportCmd v;
    v.rect = Rect{int(data->dwX), int(data->dwY), int(data->dwX + data->dwWidth), int(data->dwY + data->dwHeight)};
    v.minZ = data->dvMinZ;
    v.maxZ = data->dvMaxZ;
    win::Context::get().record.viewport(v);
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetViewport(D3DVIEWPORT7* data) {
    win::Guard guard;
    if (!data) return DDERR_INVALIDPARAMS;
    *data = viewport_;
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetMaterial(D3DMATERIAL7*) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::SetLight(DWORD, D3DLIGHT7*) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::LightEnable(DWORD, WINBOOL) { return D3D_OK; }

// 0.88: the render and stage states without the global lock - the game sets them on its one thread tens of
// thousands of times a frame (eight or nine stage states before each of its draws), and they touch only this device's
// own state, which only its methods read on that thread: the lock's entry and exit cost about as much as the game's
// own work a frame ([debug] profile on 0.87).
HRESULT STDMETHODCALLTYPE Device7::SetRenderState(D3DRENDERSTATETYPE state, DWORD value) {
    states_.setRender(uint32_t(state), value);
    scene_.renderState(uint32_t(state), value);
    static bool keyLogged = false;
    if (uint32_t(state) == d3d7::RS_COLORKEYENABLE && value && !keyLogged) {  // spec §4.5
        LOG_INFO("COLORKEYENABLE on: only textures created with a colour key drop their keyed texels");
        keyLogged = true;
    }
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetRenderState(D3DRENDERSTATETYPE state, LPDWORD value) {
    if (!value) return DDERR_INVALIDPARAMS;
    *value = states_.render(uint32_t(state));
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::PreLoad(IDirectDrawSurface7*) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::DrawPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, void* vertices, DWORD vertexCount,
                                                DWORD) {
    win::Guard guard;
    return record(type, fvf, vertices, vertexCount, nullptr, 0);
}

HRESULT STDMETHODCALLTYPE Device7::DrawIndexedPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, void* vertices,
                                                       DWORD vertexCount, WORD* indices, DWORD indexCount, DWORD) {
    win::Guard guard;
    if (!indices || indexCount == 0) return DDERR_INVALIDPARAMS;
    return record(type, fvf, vertices, vertexCount, indices, indexCount);
}

HRESULT Device7::record(D3DPRIMITIVETYPE type, DWORD fvf, const void* vertices, DWORD vertexCount,
                        const WORD* indices, DWORD indexCount) {
    if (fvf != d3d7::FVF_TL1 && fvf != d3d7::FVF_TL2)
        return notImplemented("IDirect3DDevice7::DrawPrimitive (a vertex format other than 0x1C4 and 0x2C4)");
    if (!vertices || vertexCount == 0) return DDERR_INVALIDPARAMS;
    win::Context& c = win::Context::get();
    DrawCmd cmd;
    bool hasTexture[2] = {false, false};
    for (int i = 0; i < 2; ++i) {
        Surface7* t = textures_[i];
        if (!t) continue;
        t->sync(c.record);  // the texture's changes go before the draw that samples it
        hasTexture[i] = true;
        cmd.texture[i] = t->gpuTexture();
        cmd.keyed[i] = t->colorKeyed();
    }
    win::crisp::tileDraw(textures_[0], cmd);  // stage 6: a window tile carries its twin
    DrawSetup setup = resolveDrawSetup(states_, false, hasTexture);  // the target has no alpha
    setup.ui = scene_.ui();
    cmd.state = c.record.addState(setup);
    cmd.primitive = uint32_t(type);
    cmd.fvf = fvf;
    // 0.27: the level 99 aura softer - its vertices' colour and alpha, in a copy (the game's stay as they are).
    // 0.28: the renderer learns what effect a draw is of (the fireflies are drawn anew).
    const effect_exe::Tag tag = win::drawnEffect(vertices);
    // 0.63: /aura at once - the game reads it only as an aura is made (a map entered): its ring and bubbles go the
    // moment it is off; the circles stay, as the game shows them.
    if (tag.auraExtra && c.enhanced() && win::auraOff()) return D3D_OK;
    cmd.effect = uint8_t(tag.kind);
    cmd.effectSeed = tag.seed;
    const EffectSettings fx = c.effects();
    // 0.37: the skills likewise, by their own slider.
    const bool aura = fx.aura && fx.auraBrightness < 100 && tag.kind == effect_exe::Kind::Aura;
    const bool skill = fx.skillsSoft && fx.skillBrightness < 100 && tag.kind == effect_exe::Kind::Skill;
    if ((aura || skill) && c.enhanced()) {
        static bool logged = false;
        if (!logged && aura)
            LOG_INFO("effects: a level 99 aura drawn at %d %% of the game's brightness", fx.auraBrightness);
        logged = logged || aura;
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        soft_.assign(bytes, bytes + size_t(vertexCount) * FrameRecord::stride(fvf));
        effect_exe::soften(soft_.data(), vertexCount, FrameRecord::stride(fvf),
                           aura ? fx.auraBrightness : fx.skillBrightness,
                           effect_exe::softenFor(setup.blend, setup.srcBlend, setup.dstBlend));
        vertices = soft_.data();
    }
    // 0.93: a monster, a character, an NPC or an item coming into view fades in - its vertices' alpha, in a copy.
    // 0.95: a card lying on the ground sparkles - a star drawn after it.
    bool sparkle = false;
    glint::Vertex star[8];
    float flash = 0.0f;  // 0.96: a monster just hit - drawn once more, adding light
    if ((fx.appear || fx.glint || fx.hitFlash) && c.enhanced() && !setup.ui && fvf == d3d7::FVF_TL1 &&
        vertexCount == 4 && !indices && textures_[0]) {
        const auto* v = static_cast<const uint8_t*>(vertices);
        float x = 0, y = 0, u = 0, w = 0, z = 0, rhw = 0;
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
        for (DWORD i = 0; i < 4; ++i) {
            float f[4], t[2];
            std::memcpy(f, v + i * 32, sizeof f);
            std::memcpy(t, v + i * 32 + 24, sizeof t);
            x += f[0] / 4;
            y += f[1] / 4;
            z += f[2] / 4;
            rhw += f[3] / 4;
            u += t[0] / 4;
            w += t[1] / 4;
            x0 = std::min(x0, f[0]);
            x1 = std::max(x1, f[0]);
            y0 = std::min(y0, f[1]);
            y1 = std::max(y1, f[1]);
        }
        const Surface7* tex = textures_[0];
        bool card = false;
        const uint8_t kind = tex->spriteAt(u * float(tex->width()), w * float(tex->height()), &card);
        const bool sprite = kind >= uint8_t(sprite_exe::Source::Monster) && kind <= uint8_t(sprite_exe::Source::Item);
        // Its alpha must reach the picture: blended, the vertices' alpha times the texture's.
        const bool fades = setup.blend && setup.stage[0].alphaOp == d3d7::TOP_MODULATE;
        static bool told = false;
        if (fx.appear && sprite && !told) {
            told = true;
            LOG_INFO("effects: sprites fade in as they come into view%s (blend %d, alpha op %u)",
                     fades ? "" : " - not this one: its alpha is not the vertices'", setup.blend ? 1 : 0,
                     unsigned(setup.stage[0].alphaOp));
        }
        float shown = 1.0f;
        if (fx.appear && sprite && fades) {
            shown = c.appear.alpha(kind, x, y, std::max(x1 - x0, y1 - y0), c.nowMs());
            if (shown < 1.0f) {
                if (vertices != soft_.data()) soft_.assign(v, v + 4 * 32);
                for (int i = 0; i < 4; ++i) {
                    uint8_t& alpha = soft_[size_t(i) * 32 + 19];  // the diffuse colour's alpha (ARGB)
                    alpha = uint8_t(float(alpha) * shown + 0.5f);
                }
                vertices = soft_.data();
            }
        }
        if (fx.hitFlash && kind == uint8_t(sprite_exe::Source::Monster))
            flash = c.flashes.strength(x0, x1, y0, y1, c.nowMs()) * shown;
        if (fx.glint && card && kind == uint8_t(sprite_exe::Source::Item)) {
            const uint32_t seed = uint32_t(int(x) / 8 * 7919 + int(y) / 8 * 104729);
            const float flare = glint::flare(c.nowMs() / 1000.0, seed) * shown;
            if (flare > 0.01f) {
                const float size = std::max(x1 - x0, 8.0f) * 1.1f;
                glint::star(x + (x1 - x0) * 0.18f, y0 + (y - y0) * 0.6f, z, rhw, size * (0.6f + 0.4f * flare), flare,
                            star);
                sparkle = true;
            }
        }
    }
    c.record.draw(cmd, vertices, vertexCount, indices, indices ? indexCount : 0);
    if (flash > 0.01f) {  // 0.96: the sprite's shape once more in white, added, as strong as the flash is now
        if (vertices != soft_.data()) {
            const auto* v = static_cast<const uint8_t*>(vertices);
            soft_.assign(v, v + 4 * 32);
        }
        for (int i = 0; i < 4; ++i) {
            uint8_t* colour = &soft_[size_t(i) * 32 + 16];  // the diffuse colour (BGRA bytes): white, the flash's alpha
            colour[0] = colour[1] = colour[2] = 255;
            colour[3] = uint8_t(std::min(255.0f, 255.0f * flash * 0.85f));
        }
        DrawSetup add = setup;
        add.blend = true;
        add.srcBlend = d3d7::BLEND_SRCALPHA;
        add.dstBlend = d3d7::BLEND_ONE;
        add.zWrite = false;
        add.fog = false;
        add.stage[0].colorOp = d3d7::TOP_SELECTARG1;  // white where the sprite is (its alpha times the flash's)
        add.stage[0].colorArg1 = d3d7::TA_DIFFUSE;
        add.stage[1].colorOp = add.stage[1].alphaOp = d3d7::TOP_DISABLE;
        DrawCmd again = cmd;
        again.state = c.record.addState(add);
        c.record.draw(again, soft_.data(), 4, nullptr, 0);
    }
    if (sparkle) {  // 0.95: adding light, untextured (white), tested against the depth but not written to it
        DrawSetup add = setup;
        add.blend = true;
        add.srcBlend = d3d7::BLEND_SRCALPHA;
        add.dstBlend = d3d7::BLEND_ONE;
        add.alphaTest = false;
        add.zWrite = false;
        add.fog = false;
        add.colorKey = false;
        add.stage[0].colorOp = add.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
        add.stage[0].colorArg1 = add.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
        add.stage[1].colorOp = add.stage[1].alphaOp = d3d7::TOP_DISABLE;
        DrawCmd bar;
        bar.state = c.record.addState(add);
        bar.primitive = d3d7::PT_TRIANGLESTRIP;
        bar.fvf = d3d7::FVF_TL1;
        c.record.draw(bar, star, 4, nullptr, 0);
        c.record.draw(bar, star + 4, 4, nullptr, 0);
    }
    c.flushIfFull();  // the textures this draw sent may have filled the record
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetTexture(DWORD stage, IDirectDrawSurface7** surface) {
    win::Guard guard;
    if (!surface || stage >= d3d7::kStages) return DDERR_INVALIDPARAMS;
    *surface = textures_[stage];
    if (textures_[stage]) textures_[stage]->AddRef();
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetTexture(DWORD stage, IDirectDrawSurface7* surface) {
    win::Guard guard;
    if (stage >= d3d7::kStages) return DDERR_INVALIDPARAMS;
    Surface7* t = Surface7::from(surface);
    if (surface && (!t || t->role() != SurfaceRole::Texture)) return DDERR_INVALIDPARAMS;
    if (t) t->AddRef();
    if (textures_[stage]) textures_[stage]->Release();
    textures_[stage] = t;  // NULL on stage 0 samples white (states.md §5)
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, LPDWORD value) {
    if (!value || stage >= d3d7::kStages) return DDERR_INVALIDPARAMS;
    *value = states_.stage(stage, uint32_t(type));
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) {
    if (stage >= d3d7::kStages) return DDERR_INVALIDPARAMS;
    states_.setStage(stage, uint32_t(type), value);
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::ValidateDevice(LPDWORD passes) {
    if (passes) *passes = 1;
    return D3D_OK;
}

}  // namespace uagfx::dx7
