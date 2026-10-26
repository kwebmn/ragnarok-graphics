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
    // the GPU starts on the frame while the client gets ready to present it; 3.0.3-beta.12: the windows' panel into it
    win::Context::get().endScene();
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
    // 0.27: the level 99 aura softer - its vertices' colour and alpha, in a copy (the game's stay as they are).
    // 0.28: the renderer learns what effect a draw is of (the fireflies are drawn anew).
    const effect_exe::Tag tag = win::drawnEffect(vertices);
    // (2.7.23's orb sprites without depth gone in 2.7.24: the game writes its laid-over sprites' depth wherever their
    // alpha is not 0 - uaRO.exe 0x40E4D0: ZWRITEENABLE on, ALPHAREF 1 - so Ruwach's sprites hide each other, a line;
    // without it they piled up into a ball, the user: «всё равно какой-то шар крутится». Their light is a ground
    // light instead: effect_light::kGroundLight.)
    cmd.state = c.record.addState(setup);
    cmd.primitive = uint32_t(type);
    cmd.fvf = fvf;
    // 0.63: /aura at once - the game reads it only as an aura is made (a map entered): its ring and bubbles go the
    // moment it is off; the circles stay, as the game shows them.
    if (tag.auraExtra && c.enhanced() && win::auraOff()) return D3D_OK;
    cmd.effect = uint8_t(tag.kind);
    cmd.effectSeed = tag.seed;
    cmd.effectId = tag.id;  // 2.7.21: Ruwach's orbs light by it
    const EffectSettings& fx = c.effects();  // 2.5.11: made once a change, not each draw
    // 0.37: the skills likewise, by their own slider. (2.7.37's night that made them softer still gone in 2.7.46 -
    // with 2.7.38's soft adding they do not burn white, and the tester found them «тусклые»)
    const bool aura = fx.aura && fx.auraBrightness < 100 && tag.kind == effect_exe::Kind::Aura;
    const int skillPercent = std::clamp(fx.skillBrightness, 0, 100);
    // 2.7.39 (the tester: «как светится персонаж во время каста не устраивает»): a casting quieter still
    const int drawPercent = effect_exe::isCasting(tag.id) ? effect_exe::castPercent(skillPercent) : skillPercent;
    const bool skill = fx.skillsSoft && drawPercent < 100 && tag.kind == effect_exe::Kind::Skill;
    if ((aura || skill) && c.enhanced()) {
        static bool logged = false;
        if (!logged && aura)
            LOG_INFO("effects: a level 99 aura drawn at %d %% of the game's brightness", fx.auraBrightness);
        logged = logged || aura;
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        soft_.assign(bytes, bytes + size_t(vertexCount) * FrameRecord::stride(fvf));
        effect_exe::soften(soft_.data(), vertexCount, FrameRecord::stride(fvf),
                           aura ? fx.auraBrightness : drawPercent,
                           aura ? effect_exe::softenFor(setup.blend, setup.srcBlend, setup.dstBlend)
                                : effect_exe::softenSkill(setup.blend, setup.srcBlend, setup.dstBlend));
        vertices = soft_.data();
    }
    if (c.enhanced()) {  // 2.7.37: what of the bright draws is made softer, now and then in the log
        if (tag.kind == effect_exe::Kind::Skill)
            ++skillDraws_;
        else if (!setup.ui && setup.blend && setup.dstBlend == d3d7::BLEND_ONE && tag.kind == effect_exe::Kind::None) {
            ++plainAdds_;
            notePlainAdd(static_cast<const uint8_t*>(vertices), vertexCount, FrameRecord::stride(fvf), uint32_t(type),
                         setup.srcBlend);
        } else if (setup.ui && setup.blend) {
            noteOver(static_cast<const uint8_t*>(vertices), vertexCount, FrameRecord::stride(fvf), uint32_t(type),
                     setup.srcBlend, setup.dstBlend);
        }
        const ULONGLONG now = GetTickCount64();
        if (now - skillsTold_ >= 10000) {
            if (skillDraws_ || plainAdds_)
                LOG_INFO("effects: in 10 s %u draws of skills at %d %% of the game's brightness; %u draws adding light "
                         "of no effect known, as the game draws them",
                         skillDraws_, fx.skillsSoft ? std::min(skillPercent, 100) : 100, plainAdds_);
            if (widestAdd_.area >= 0.0f) {
                const PlainAdd& w = widestAdd_;
                LOG_INFO("effects: the widest of them %.0f,%.0f-%.0f,%.0f of the game's %ux%u, %u vertices "
                         "(primitive %u), colours #%08X-#%08X, source blend %u, texture %dx%d, depth %.4f-%.4f; %u of "
                         "them covered half the picture or more",
                         w.x0, w.y0, w.x1, w.y1, unsigned(viewport_.dwWidth), unsigned(viewport_.dwHeight), w.vertices,
                         w.primitive, w.colourMin, w.colourMax, w.srcBlend, w.texWidth, w.texHeight, w.z0, w.z1,
                         wideAdds_);
            }
            if (wideOvers_) {
                const PlainAdd& w = widestOver_;
                LOG_INFO("effects: %u draws laid over the picture with the interface covering half of it or more; the "
                         "widest %.0f,%.0f-%.0f,%.0f of the game's %ux%u, %u vertices (primitive %u), colours "
                         "#%08X-#%08X, blend %u/%u, texture %dx%d",
                         wideOvers_, w.x0, w.y0, w.x1, w.y1, unsigned(viewport_.dwWidth), unsigned(viewport_.dwHeight),
                         w.vertices, w.primitive, w.colourMin, w.colourMax, w.srcBlend, w.dstBlend, w.texWidth,
                         w.texHeight);
            }
            skillDraws_ = plainAdds_ = wideAdds_ = wideOvers_ = 0;
            widestAdd_ = widestOver_ = PlainAdd{};
            skillsTold_ = now;
        }
    }
    // 2.7.11, diagnostics: a hit no monster took (win/hit_hook.cpp) - each quad drawn about its place now, in the log
    // (2.7.12: the sprites' only - the ground's cells took ten of the sixteen lines)
    if (c.hitProbe.frames > 0 && c.hitProbe.lines < 16 && vertexCount == 4 && fvf == d3d7::FVF_TL1) {
        const size_t stride = FrameRecord::stride(fvf);
        const auto* v = static_cast<const uint8_t*>(vertices);
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, u = 0, w = 0;
        for (DWORD i = 0; i < 4; ++i) {
            float f[2], t[2];
            std::memcpy(f, v + i * stride, sizeof f);
            std::memcpy(t, v + i * stride + 24, sizeof t);
            x0 = std::min(x0, f[0]);
            x1 = std::max(x1, f[0]);
            y0 = std::min(y0, f[1]);
            y1 = std::max(y1, f[1]);
            u += t[0] / 4;
            w += t[1] / 4;
        }
        const float px = c.hitProbe.x, py = c.hitProbe.y, pad = std::max((x1 - x0) * 0.15f, 6.0f);
        if (px >= x0 - pad && px <= x1 + pad && py >= y0 - 300.0f && py <= y1 + 300.0f) {
            ++c.hitProbe.lines;
            bool card = false;
            const uint8_t kind = textures_[0] ? textures_[0]->spriteAt(u * float(textures_[0]->width()),
                                                                         w * float(textures_[0]->height()), &card)
                                              : 0;
            LOG_INFO("hit flash probe: fvf %X, %u vertices, %u indices, %s, blend %d, %s texture %dx%d, kind %s, box "
                     "%.0f..%.0f x %.0f..%.0f",
                     unsigned(fvf), unsigned(vertexCount), unsigned(indices ? indexCount : 0),
                     setup.ui ? "interface" : "world", setup.blend ? 1 : 0, textures_[0] ? "a" : "no",
                     textures_[0] ? textures_[0]->width() : 0, textures_[0] ? textures_[0]->height() : 0,
                     sprite_exe::sourceName(kind), x0, x1, y0, y1);
        }
    }
    // 0.95: a card lying on the ground sparkles - a star drawn after it. (2.7.10: the sprites' fade-in as they came
    // into view - 0.93 - gone at the user's word.)
    float flash = 0.0f;  // 0.96: a monster just hit - drawn once more, adding light
    if ((fx.glint || fx.hitFlash) && c.enhanced() && !setup.ui && fvf == d3d7::FVF_TL1 &&
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
        if (fx.hitFlash && kind == uint8_t(sprite_exe::Source::Monster)) {
            c.monsters.note(hit_flash::Box{x0, x1, y0, y1});  // 2.7.10: the next frame's hits find it here
            flash = c.flashes.strength(x0, x1, y0, y1, c.nowMs());
        }
        if (fx.glint && card && kind == uint8_t(sprite_exe::Source::Item)) {
            const uint32_t seed = uint32_t(int(x) / 8 * 7919 + int(y) / 8 * 104729);
            const float flare = glint::flare(c.nowMs() / 1000.0, seed);
            if (flare > 0.01f && c.sparkles.size() < 64) {
                // 0.95: adding light, untextured (white), tested against the depth but not written to it. 2.7.14:
                // a sparkle, not a cross, drawn once the world's sprites are (Context::drawSparkles) - drawn at once,
                // the card's picture laid over it hid it (the tester: «под картой»)
                win::Context::Sparkle s;
                s.setup = setup;
                s.setup.blend = true;
                s.setup.srcBlend = d3d7::BLEND_SRCALPHA;
                s.setup.dstBlend = d3d7::BLEND_ONE;
                s.setup.alphaTest = false;
                s.setup.zWrite = false;
                s.setup.fog = false;
                s.setup.colorKey = false;
                s.setup.stage[0].colorOp = s.setup.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
                s.setup.stage[0].colorArg1 = s.setup.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
                s.setup.stage[1].colorOp = s.setup.stage[1].alphaOp = d3d7::TOP_DISABLE;
                // 2.7.15: twice the card's width, on its upper right corner (on Prontera's bright paving a thin needle
                // above it; on its middle the glow whited the card out)
                const float size = std::max(x1 - x0, 8.0f) * 2.0f;
                glint::sparkle(x + (x1 - x0) * 0.3f, y - (y1 - y0) * 0.3f, z, rhw, size * (0.6f + 0.4f * flare), flare,
                               s.v);
                c.sparkles.push_back(s);
            }
        }
    }
    // 2.7.25 (the user: «можно сделать так, чтобы тот шарик был в начале того синего шлейфа?»): Ruwach's sprites (its
    // effect 33: the orb's trail round the hero) - the newest of each caster's is the head of the trail: a small ball
    // of light there once the world is drawn (glint::orb). 2.7.26 (the user: «на том же круге, что полоса»): its
    // shadows on the ground are not - 2.7.25's ball sat on one. 2.7.27: told by their primitives' height over the
    // effect (Tag::raised: the orbs 10 up, the shadows 0) - 2.7.26 by the sprite cache's cells, which know no
    // effect's file (its log: none of 18 the orb's own)
    if (!setup.ui && setup.blend && tag.id == 33 && fvf == d3d7::FVF_TL1 && vertexCount == 4 && c.enhanced() &&
        fx.fireLight && fx.lightOrbs) {
        const size_t stride = FrameRecord::stride(fvf);
        const auto* v = static_cast<const uint8_t*>(vertices);
        float x = 0, y = 0, z = 0, rhw = 0, x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
        uint32_t alpha = 0;
        for (DWORD i = 0; i < 4; ++i) {
            float f[4];
            uint32_t diffuse = 0;
            std::memcpy(f, v + i * stride, sizeof f);
            std::memcpy(&diffuse, v + i * stride + 16, sizeof diffuse);
            x += f[0] / 4, y += f[1] / 4, z += f[2] / 4, rhw += f[3] / 4;
            x0 = std::min(x0, f[0]), x1 = std::max(x1, f[0]), y0 = std::min(y0, f[1]), y1 = std::max(y1, f[1]);
            alpha = std::max(alpha, diffuse >> 24);
        }
        ++c.orbSprites;
        if (tag.raised) {
            ++c.orbOwn;
            c.orbHeads.note(x, y, z, rhw, std::max(x1 - x0, y1 - y0), float(alpha) / 255.0f);
        }
        c.orbSetup = setup;  // adding light, untextured, tested against the depth but not written to it (as a sparkle)
        c.orbSetup.srcBlend = d3d7::BLEND_SRCALPHA;
        c.orbSetup.dstBlend = d3d7::BLEND_ONE;
        c.orbSetup.alphaTest = c.orbSetup.zWrite = c.orbSetup.fog = c.orbSetup.colorKey = false;
        c.orbSetup.stage[0].colorOp = c.orbSetup.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
        c.orbSetup.stage[0].colorArg1 = c.orbSetup.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
        c.orbSetup.stage[1].colorOp = c.orbSetup.stage[1].alphaOp = d3d7::TOP_DISABLE;
    }
    // 2.7.14: the world drawn - the cards' sparkles over it; 2.7.25: and the orbs' heads
    if (setup.ui && (!c.sparkles.empty() || !c.orbHeads.heads().empty())) c.drawSparkles();
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
        again.effect = uint8_t(effect_exe::Kind::HitFlash);  // 2.7.13: no soft particle - it went out wholly
        again.effectSeed = 0;
        again.effectId = 0;
        c.record.draw(again, soft_.data(), 4, nullptr, 0);
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

Device7::PlainAdd Device7::measure(const uint8_t* vertices, uint32_t count, uint32_t stride) {
    PlainAdd a;
    a.x0 = a.y0 = a.z0 = 1e30f;
    a.x1 = a.y1 = a.z1 = -1e30f;
    uint8_t lo[4] = {255, 255, 255, 255}, hi[4] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* v = vertices + size_t(i) * stride;
        float x, y, z;
        uint32_t colour;
        std::memcpy(&x, v, 4);
        std::memcpy(&y, v + 4, 4);
        std::memcpy(&z, v + 8, 4);
        std::memcpy(&colour, v + 16, 4);
        a.x0 = std::min(a.x0, x), a.x1 = std::max(a.x1, x);
        a.y0 = std::min(a.y0, y), a.y1 = std::max(a.y1, y);
        a.z0 = std::min(a.z0, z), a.z1 = std::max(a.z1, z);
        for (int k = 0; k < 4; ++k) {
            const uint8_t c = uint8_t(colour >> (24 - 8 * k));
            lo[k] = std::min(lo[k], c), hi[k] = std::max(hi[k], c);
        }
    }
    for (int k = 0; k < 4; ++k) {
        a.colourMin |= uint32_t(lo[k]) << (24 - 8 * k);
        a.colourMax |= uint32_t(hi[k]) << (24 - 8 * k);
    }
    a.area = std::max(0.0f, a.x1 - a.x0) * std::max(0.0f, a.y1 - a.y0);
    a.vertices = count;
    return a;
}

void Device7::notePlainAdd(const uint8_t* vertices, uint32_t count, uint32_t stride, uint32_t primitive,
                           uint32_t srcBlend) {
    PlainAdd a = measure(vertices, count, stride);
    if (2.0f * a.area >= float(viewport_.dwWidth) * float(viewport_.dwHeight)) ++wideAdds_;
    if (a.area <= widestAdd_.area) return;
    a.primitive = primitive;
    a.srcBlend = srcBlend;
    if (const Surface7* t = textures_[0]) a.texWidth = t->width(), a.texHeight = t->height();
    widestAdd_ = a;
}

void Device7::noteOver(const uint8_t* vertices, uint32_t count, uint32_t stride, uint32_t primitive,
                       uint32_t srcBlend, uint32_t dstBlend) {
    PlainAdd a = measure(vertices, count, stride);
    if (2.0f * a.area < float(viewport_.dwWidth) * float(viewport_.dwHeight)) return;  // a window, a letter
    ++wideOvers_;
    if (a.area <= widestOver_.area) return;
    a.primitive = primitive;
    a.srcBlend = srcBlend;
    a.dstBlend = dstBlend;
    if (const Surface7* t = textures_[0]) a.texWidth = t->width(), a.texHeight = t->height();
    widestOver_ = a;
}

}  // namespace uagfx::dx7
