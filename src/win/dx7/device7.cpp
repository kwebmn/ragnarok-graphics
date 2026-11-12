#include "win/dx7/device7.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/device_caps.hpp"
#include "core/frame.hpp"
#include "core/glint.hpp"
#include "core/mage_looks.hpp"
#include "core/version.hpp"
#include "core/layout.hpp"
#include "core/sprite_exe.hpp"
#include "win/context.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dx7/caps.hpp"
#include "win/dx7/formats.hpp"
#include "win/dx7/surface7.hpp"
#include "win/effect_hook.hpp"
#include "win/game_camera.hpp"
#include "win/ground_marks.hpp"
#include "win/ground_way.hpp"
#include "win/log.hpp"
#include "win/net_predict.hpp"
#include "win/net_smooth.hpp"
#include "win/paths.hpp"
#include "win/light_skills.hpp"
#include "win/sharp_frames.hpp"
#include "win/sharp_specks.hpp"
#include "win/skill_delay_watch.hpp"
#include "win/buff_overlay.hpp"
#include "win/combo_watch.hpp"
#include "win/windows_draw.hpp"

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
    portals_.frame();  // 3.0.13-beta.15
    castLooks_.frame();  // 3.0.15-beta.2
    {  // 3.0.13-beta.30: where the character's body is on the screen, for its silhouette (core/model_fade.hpp holeOf)
        win::Context& c = win::Context::get();
        float view[12], at[3], hole[8] = {};
        // 3.0.13-beta.30: [effects] silhouette (fade_models, .17-.29, gone in .31: «чтото не то, возвращай как было»)
        const uint8_t flags = c.effects().silhouette ? 2 : 0;
        const bool tintOn = c.effects().bodyTints;  // 3.0.16-beta.13
        hero_ = modelfade::Focus{};
        tint_ = afterimage::Tint{};
        if ((flags || tintOn) && c.enhanced() && c.screenWidth > 0 && win::readCameraAndTarget(view, at)) {
            const camera::Projection p = camera::projectionFor(c.screenWidth, c.screenHeight);
            hero_ = modelfade::focusOf(p, view, at);
            if (flags) modelfade::holeOf(hero_, hole);
            if (hole[3] > 0.0f && !fadeTold_) {
                fadeTold_ = true;
                LOG_INFO("effects: the character's place told the renderer ([effects] silhouette)");
            }
            if (tintOn) tint_ = afterimage::tintOf(win::buffoverlay::timers().on(86), win::combowatch::steelBody());
            if (tint_.strength > 0 && !tintTold_) {
                tintTold_ = true;
                LOG_INFO("effects: the character's body tinted (%s, [effects] body_tints)",
                         tint_.r == 255 ? "Fury" : "Steel Body");
            }
        }
        c.setSeeThrough(hole, flags);
    }
    win::Context::get().markInterfaceEnd(win::interfaceFaces());  // 3.0.11-beta.190: the game's interface list
    win::groundway::newFrame();    // 3.0.11-beta.31: the way on the ground drawn once in this frame
    win::groundmarks::newFrame();  // 3.0.11-beta.87: the target's ring too
    win::netsmoothwin::beginScene();  // 3.0.11-beta.123: the server's position fixes drawn gliding
    win::netpredictwin::beginScene();  // 3.0.11-beta.130: the own walk guessed at the click
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::EndScene() {
    win::Guard guard;
    ++captureFrame_;  // 3.0.15-beta.1
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

// 3.0.15-beta.1: a skill effect's draw into uaro-graphics\logs\uaro-effects.txt (core/effect_capture.hpp) - betas
// only, the file begun anew in each game
void Device7::captureEffect(const effect_exe::Tag& tag, const DrawSetup& setup, DWORD fvf, const void* vertices,
                            DWORD vertexCount, const WORD* indices, DWORD indexCount) {
    if (!capture_.take(int(tag.id), captureFrame_)) return;
    const std::string path = layout::join(win::gameDir(), layout::kEffectCapture);
    if (captureState_ == 0) {
        if (!std::strstr(kVersion, "beta")) {
            captureState_ = -1;
            return;
        }
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) {
            captureState_ = -1;
            LOG_INFO("effect capture: %s could not be opened", path.c_str());
            return;
        }
        std::fprintf(f, "uaRO Graphics %s - the skills' effects as the game draws them (core/effect_capture.hpp)\n",
                     kVersion);
        std::fclose(f);
        captureState_ = 1;
        LOG_INFO("effect capture: the skills' effects' first frames into %s", path.c_str());
    }
    const size_t stride = FrameRecord::stride(fvf);
    const auto* bytes = static_cast<const uint8_t*>(vertices);
    std::vector<effect_capture::Vertex> v(std::min<DWORD>(vertexCount, 256));
    for (size_t i = 0; i < v.size(); ++i) {
        const uint8_t* p = bytes + i * stride;
        std::memcpy(&v[i].x, p, 16);  // x, y, z, rhw
        std::memcpy(&v[i].diffuse, p + 16, 4);
        std::memcpy(&v[i].specular, p + 20, 4);
        std::memcpy(&v[i].u, p + 24, 8);
    }
    Surface7* t = textures_[0];
    const uint32_t id = t ? t->gpuTexture() : 0;
    const int w = t ? t->width() : 0, h = t ? t->height() : 0;
    const int at = capture_.offset(int(tag.id), captureFrame_);
    std::string text = at == 0 && captureFirst_.insert(int(tag.id)).second
                           ? effect_capture::firstText(int(tag.id), win::skilldelaywin::lastSkill())
                           : std::string();
    text += effect_capture::drawText(int(tag.id), at, setup, id, w, h, v.data(), int(v.size()), indices,
                                                int(std::min<DWORD>(indices ? indexCount : 0, 768)));
    std::vector<uint32_t> argb;
    if (t && capture_.textureTold(id) && t->topArgb(argb)) text += effect_capture::textureText(id, w, h, argb.data());
    FILE* f = std::fopen(path.c_str(), "ab");
    if (!f) return;
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    capture_.wrote(text.size());
}

HRESULT Device7::record(D3DPRIMITIVETYPE type, DWORD fvf, const void* vertices, DWORD vertexCount,
                        const WORD* indices, DWORD indexCount) {
    if (fvf != d3d7::FVF_TL1 && fvf != d3d7::FVF_TL2)
        return notImplemented("IDirect3DDevice7::DrawPrimitive (a vertex format other than 0x1C4 and 0x2C4)");
    if (!vertices || vertexCount == 0) return DDERR_INVALIDPARAMS;
    win::Context& c = win::Context::get();
    DrawCmd cmd;
    // 0.28: the renderer learns what effect a draw is of (the fireflies are drawn anew). 3.0.18-beta.181: found first
    const effect_exe::Tag tag = win::drawnEffect(vertices);
    if (tag.kind == effect_exe::Kind::Skill) win::lightskillswin::drawn();  // 3.0.18-beta.186: the crowd of skills
    // 3.0.18-beta.181 (Storm Gust), .184 (every skill: EffectSettings::sharpSpecks, any skills' look but the
    // original): a skill's texture looked at before it goes up (its texels then as the game last wrote them) - a sheet
    // of specks (the snow, the sparkles) gets a copy at real resolution (win/sharp_specks)
    const EffectSettings& look0 = c.effects();
    const bool sharpSpecks = tag.kind == effect_exe::Kind::Skill && textures_[0] && c.enhanced() &&
                             look0.sharpSpecks && !textures_[0]->spriteCache();  // the sprites' cells: as they are
    if (sharpSpecks) {
        Surface7* t = textures_[0];
        if (win::sharpspecks::wanted(t->gpuTexture(), t->topDirty()) && t->topArgb(specks_) &&
            specks_.size() == size_t(t->width()) * size_t(t->height()))
            win::sharpspecks::note(t->gpuTexture(), specks_.data(), t->width(), t->height(), unsigned(tag.id));
    }
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
    // 3.0.18-beta.229 (R-H2CQCJ «негатив»): an effect's inverted texture (the game's preset 6) kept to this draw - the
    // game never sets it back ([effects] fix_negative, core DeviceStates::dropComplement)
    if (c.enhanced() && c.effects().fixNegative)
        if (const uint8_t back = states_.dropComplement(); back && negativeTold_ < 5) {
            ++negativeTold_;
            LOG_INFO("effects: an inverted texture (the game's preset 6) put back after its draw - effect kind %d, "
                     "id %d, stages %u; the world after it not a negative ([effects] fix_negative)",
                     int(tag.kind), int(tag.id), unsigned(back));
        }
    setup.ui = scene_.ui();
    // 0.27: the level 99 aura softer - its vertices' colour and alpha, in a copy (the game's stay as they are)
    // 3.0.15-beta.2: [effects] new_casting - the casting's magic circle on the ground, drawn before its walls and
    // before anything here uses the scratch copies (it goes through record too)
    if (!castCircleNext_ && fvf == d3d7::FVF_TL1 && !setup.ui && tag.kind == effect_exe::Kind::Skill &&
        effect_exe::isCasting(tag.id) && c.effects().mageEffects && c.effects().mageCasting && c.enhanced()) {
        uint8_t quad[4 * 32];
        const uint32_t key = castlook::keyOf(tag.owner, tag.id);
        if (!tag.owner) castLooks_.note(static_cast<const uint8_t*>(vertices), vertexCount, 32, key);  // .6
        if (castLooks_.circleQuad(key, quad)) {
            castCircleNext_ = true;
            record(D3DPT_TRIANGLESTRIP, fvf, quad, 4, nullptr, 0);
            castCircleNext_ = false;
            if (!castTold_) {
                castTold_ = true;
                LOG_INFO("effects: the casting's new look - its flame held low, a magic circle under it "
                         "([effects] new_casting)");
            }
        }
    }
    // (2.7.23's orb sprites without depth gone in 2.7.24: the game writes its laid-over sprites' depth wherever their
    // alpha is not 0 - uaRO.exe 0x40E4D0: ZWRITEENABLE on, ALPHAREF 1 - so Ruwach's sprites hide each other, a line;
    // without it they piled up into a ball, the user: «всё равно какой-то шар крутится». Their light is a ground
    // light instead: effect_light::kGroundLight.)
    // 3.0.18-beta.178: [display] crisp_numbers - a damage digit's, Miss's, Critical's, Lucky's frame from its sharp
    // copy (win/sharp_frames): its texture and coordinates, the quad and its colours the game's
    if (textures_[0] && c.config.crispNumbers && textures_[0]->anySpriteNumber() && setup.stage[0].texCoord == 0) {
        const size_t stride = FrameRecord::stride(fvf);
        const auto* v = static_cast<const uint8_t*>(vertices);
        float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
        for (DWORD i = 0; i < vertexCount; ++i) {
            float uv[2];
            std::memcpy(uv, v + i * stride + 24, sizeof uv);
            u0 = std::min(u0, uv[0]), u1 = std::max(u1, uv[0]), v0 = std::min(v0, uv[1]), v1 = std::max(v1, uv[1]);
        }
        int texW = 0, texH = 0;
        if (const sprite_cells::Cell* n = textures_[0]->spriteNumberAt((u0 + u1) / 2, (v0 + v1) / 2, texW, texH))
            win::sharpframes::swap(*n, texW, texH, cmd, setup, vertices, vertexCount, fvf, sharp_);
    }
    if (sharpSpecks && !setup.ui)  // 3.0.18-beta.181: its specks from their copy (.184: every skill, every look)
        win::sharpspecks::swap(cmd, setup,
                               look0.skillLooks == kSkillImproved ? "improved original"
                               : look0.skillLooks == kSkillSoft   ? "redrawn, soft"
                                                                  : "redrawn");
    cmd.state = c.record.addState(setup);
    cmd.primitive = uint32_t(type);
    cmd.fvf = fvf;
    // 0.63: /aura at once - the game reads it only as an aura is made (a map entered): its ring and bubbles go the
    // moment it is off; the circles stay, as the game shows them.
    if (tag.auraExtra && c.enhanced() && win::auraOff()) return D3D_OK;
    cmd.effect = uint8_t(tag.kind);
    cmd.effectSeed = tag.seed;
    cmd.effectId = tag.id;  // 2.7.21: Ruwach's orbs light by it
    if (tag.kind == effect_exe::Kind::Skill && captureState_ >= 0)  // 3.0.15-beta.1
        captureEffect(tag, setup, fvf, vertices, vertexCount, indices, indexCount);
    const EffectSettings& fx = c.effects();  // 2.5.11: made once a change, not each draw
    // 0.37: the skills likewise, by their own slider. (2.7.37's night that made them softer still gone in 2.7.46 -
    // with 2.7.38's soft adding they do not burn white, and the tester found them «тусклые»)
    // 3.0.13-beta.15: [effects] aura_portals - the portals' boxes this frame; an aura's piece on one of the last
    // frame's made faint
    const uint32_t stride = FrameRecord::stride(fvf);
    if (fx.auraPortals && tag.kind == effect_exe::Kind::Portal && c.enhanced())
        portals_.portal(static_cast<const uint8_t*>(vertices), vertexCount, stride);
    const bool overPortal = fx.auraPortals && tag.kind == effect_exe::Kind::Aura &&
                            portals_.covers(static_cast<const uint8_t*>(vertices), vertexCount, stride);
    const int auraBase = fx.aura ? fx.auraBrightness : 100;
    const int auraPercent = overPortal ? std::min(auraBase, auraportal::kOverPercent) : auraBase;
    if (overPortal && !overPortalTold_) {
        overPortalTold_ = true;
        LOG_INFO("effects: a level 99 aura over a warp portal drawn at %d %% - the portal seen through it",
                 auraPercent);
    }
    const bool aura = auraPercent < 100 && tag.kind == effect_exe::Kind::Aura;
    const int skillPercent = std::clamp(fx.skillBrightness, 0, 100);
    // 2.7.39 (the tester: «как светится персонаж во время каста не устраивает»): a casting quieter still
    const int drawPercent = effect_exe::isCasting(tag.id) ? effect_exe::castPercent(skillPercent) : skillPercent;
    // 3.0.13-beta.20: [effects] crowd_effects - another's skill effect (on an actor neither the character nor a party
    // member here) at kCrowdPercent, laid over by alpha too
    const bool crowd = fx.crowdEffects && tag.kind == effect_exe::Kind::Skill && tag.owner && !c.friendOf(tag.owner);
    // 3.0.15-beta.3 (R-JET379: no circle, the flame faint): the new casting's own brightness, not 2.7.39's quieting -
    // softened first, its feet walls' alpha never reached core/cast_look's kBaseAlpha
    const bool ownCasting = fx.mageEffects && fx.mageCasting && effect_exe::isCasting(tag.id) && tag.owner;  // .6
    // 3.0.15-beta.8: [effects] new_storm_gust - its own brightness too
    // .11: [effects] mage_effects - the mages' skills by their element (core/mage_looks.hpp)
    // 3.0.15-beta.12: and every other class's (core/mage_looks.hpp lookOf), a switch a branch
    const magelook::Look look = magelook::lookOf(int(tag.id));
    const magelook::Element element = look.tone;
    const bool branchOn = [&] {
        switch (look.branch) {
            case magelook::Branch::Mage:
                return fx.mageEffects && (element == magelook::Element::Fire    ? fx.mageFire
                                          : element == magelook::Element::Water ? fx.mageWater
                                          : element == magelook::Element::Wind  ? fx.mageWind
                                          : element == magelook::Element::Earth ? fx.mageEarth
                                                                                : fx.mageSpirit);
            case magelook::Branch::Sword: return fx.swordEffects;
            case magelook::Branch::Acolyte: return fx.acolyteEffects;
            case magelook::Branch::Archer: return fx.archerEffects;
            case magelook::Branch::Thief: return fx.thiefEffects;
            case magelook::Branch::Merchant: return fx.merchantEffects;
            case magelook::Branch::Other: return fx.otherEffects;
            case magelook::Branch::Hits: return fx.hitEffects;  // 3.0.16-beta.1
            case magelook::Branch::Items: return fx.itemEffects;
            case magelook::Branch::Status: return fx.statusEffects;
            case magelook::Branch::Travel: return fx.travelEffects;
            case magelook::Branch::Monster: return fx.monsterEffects;
            default: return false;
        }
    }();
    const bool ownMage = tag.kind == effect_exe::Kind::Skill && branchOn;
    const bool skill = tag.kind == effect_exe::Kind::Skill && !ownCasting && !ownMage &&
                       (crowd || (fx.skillsSoft && drawPercent < 100));
    if ((aura || skill) && c.enhanced()) {
        static bool logged = false;
        if (!logged && aura)
            LOG_INFO("effects: a level 99 aura drawn at %d %% of the game's brightness", fx.auraBrightness);
        logged = logged || aura;
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        soft_.assign(bytes, bytes + size_t(vertexCount) * FrameRecord::stride(fvf));
        effect_exe::soften(soft_.data(), vertexCount, FrameRecord::stride(fvf),
                           aura    ? auraPercent
                           : crowd ? std::min(drawPercent, effect_exe::kCrowdPercent)
                                   : drawPercent,
                           aura || crowd ? effect_exe::softenFor(setup.blend, setup.srcBlend, setup.dstBlend)
                                         : effect_exe::softenSkill(setup.blend, setup.srcBlend, setup.dstBlend));
        vertices = soft_.data();
    }
    // 3.0.15-beta.2: [effects] new_casting - the casting's walls held low and drawn crisp by the shaders, its magic
    // circle on the ground drawn first (core/cast_look.hpp)
    if (castCircleNext_) {
        cmd.effectId = castlook::kCircleId;
    } else if (ownCasting && c.enhanced() && !setup.ui && fvf == d3d7::FVF_TL1 &&
               tag.kind == effect_exe::Kind::Skill && effect_exe::isCasting(tag.id) && tag.owner) {
        const uint32_t owner = castlook::keyOf(tag.owner, tag.id);
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        cast_.assign(bytes, bytes + size_t(vertexCount) * 32);
        castLooks_.shorten(cast_.data(), vertexCount, 32, owner, float(c.screenHeight) * castlook::kFlameHeight);
        vertices = cast_.data();
        cmd.effectId = castlook::kRingId;
    }
    // 3.0.15-beta.8 (Storm Gust), .11 (every mage's skill): [effects] mage_effects - a part told to the shaders by its
    // own id (its element in it); an added layer laid over instead (core/mage_looks.hpp partOf)
    if (ownMage && c.enhanced() && !setup.ui && fvf == d3d7::FVF_TL1) {
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        float x0 = 1e9f, x1 = -1e9f;
        for (DWORD i = 0; i < vertexCount; ++i) {
            float x;
            std::memcpy(&x, bytes + size_t(i) * 32, 4);
            x0 = std::min(x0, x);
            x1 = std::max(x1, x);
        }
        const magelook::Part part = magelook::styled(  // 3.0.16-beta.27: [effects] mage_soft
            magelook::partFor(int(tag.id), magelook::partOf(setup.blend, setup.srcBlend, setup.dstBlend, vertexCount,
                                                            x1 - x0, float(c.screenWidth))),
            c.effects().mageSoft);
        if (part != magelook::Part::None) {
            cmd.effectId = magelook::idOf(part, element);
            if (part != magelook::Part::Solid && part != magelook::Part::Orb && part != magelook::Part::Flame &&
                part != magelook::Part::Soft) {  // .10: the spheres still add light; .24 Fire Wall too
                DrawSetup over = setup;
                over.srcBlend = d3d7::BLEND_SRCALPHA;
                over.dstBlend = d3d7::BLEND_INVSRCALPHA;
                cmd.state = c.record.addState(over);
            }
            if (!stormTold_) {
                stormTold_ = true;
                LOG_INFO("effects: the mages' skills in their own look - laid over crisp in the element's colours, "
                         "solids faceted ([effects] mage_effects; first: effect %u)", unsigned(tag.id));
            }
        }
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
    afterimage::Tint tint;  // 3.0.16-beta.13: the character's own body tinted, drawn once more adding its colour
    if ((fx.glint || fx.hitFlash || fx.deathFlash || tint_.strength > 0) && c.enhanced() &&
        !setup.ui && fvf == d3d7::FVF_TL1 &&
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
        if ((fx.hitFlash || fx.deathFlash) && kind == uint8_t(sprite_exe::Source::Monster)) {
            c.monsters.note(hit_flash::Box{x0, x1, y0, y1});  // 2.7.10: the next frame's hits find it here
            flash = c.flashes.strength(x0, x1, y0, y1, c.nowMs());
        }
        // 3.0.16-beta.13: the character's own sprite (a player's, about its feet on the screen): its tint after it
        if (kind == uint8_t(sprite_exe::Source::Player) && hero_.known && tint_.strength > 0) {
            const float band = hero_.h * 0.6f;
            if (x >= hero_.x - band && x <= hero_.x + band && y1 >= hero_.y - hero_.h * 1.6f &&
                y0 <= hero_.y + hero_.h * 0.5f)
                tint = tint_;
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
    if (setup.ui) {
        win::groundway::draw(c.record, c.screenWidth, c.screenHeight);    // 3.0.11-beta.31: the way followed
        win::groundmarks::draw(c.record, c.screenWidth, c.screenHeight);  // 3.0.11-beta.87: the target's ring
    }
    c.record.draw(cmd, vertices, vertexCount, indices, indices ? indexCount : 0);
    if (tint.strength > 0 && flash <= 0.01f) {  // 3.0.16-beta.13: the body's tint - its colour added where it is
        flash = tint.strength;
    }
    if (flash > 0.01f) {  // 0.96: the sprite's shape once more in white, added, as strong as the flash is now
        if (vertices != soft_.data()) {
            const auto* v = static_cast<const uint8_t*>(vertices);
            soft_.assign(v, v + 4 * 32);
        }
        for (int i = 0; i < 4; ++i) {
            uint8_t* colour = &soft_[size_t(i) * 32 + 16];  // the diffuse colour (BGRA bytes): white, the flash's alpha
            colour[0] = colour[1] = colour[2] = 255;
            if (tint.strength > 0 && flash == tint.strength) {  // 3.0.16-beta.13: the tint's colour (BGRA)
                colour[0] = tint.b;
                colour[1] = tint.g;
                colour[2] = tint.r;
            }
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
