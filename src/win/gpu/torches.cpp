#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr float kReach = 45.0f;       // how far a torch's light reaches, world units (a cell is ten)
constexpr float kLightColor[3] = {1.0f, 0.58f, 0.28f};  // warm, as a wood fire
constexpr float kGlowRadius = 2.2f;  // the glow's pass, flame heights around the flame's middle
constexpr float kMiddle = 0.38f;     // the flame's middle, of its height from the base

D3D12_RECT clipped(float x0, float y0, float x1, float y1, int w, int h) {
    D3D12_RECT r{LONG(std::max(0.0f, std::floor(x0))), LONG(std::max(0.0f, std::floor(y0))),
                 LONG(std::min(float(w), std::ceil(x1))), LONG(std::min(float(h), std::ceil(y1)))};
    return r;
}

}  // namespace

// A world draw of one of the game's torch layers (core/torch.hpp) - known by its texture (Renderer::noteObjectTexture):
// true when it is not to be drawn, the glow not at all, the flame noted for applyTorches. Drawn as the game has it
// when the torches cannot be drawn anew here (no shaders, no copy of the picture) or the frame's are drawn already.
bool Renderer::Impl::takeTorch(const FrameRecord& rec, const DrawCmd& d) {
    const objects::Kind kind = objectTextures.of(d.texture[0]);
    if (kind != objects::Kind::TorchGlow && kind != objects::Kind::TorchFlame) return false;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > v.size()) return false;
    torch::Corner q[4];
    uint32_t diffuse = 0;
    for (int i = 0; i < 4; ++i) {
        const uint8_t* p = v.data() + d.vertexOffset + size_t(i) * stride;
        float xyzw[4], uv[2];
        std::memcpy(xyzw, p, sizeof xyzw);
        std::memcpy(uv, p + 24, sizeof uv);  // after the position, the diffuse and the specular colours
        q[i] = torch::Corner{xyzw[0], xyzw[1], xyzw[2], xyzw[3], uv[0], uv[1]};
        if (i == 0) std::memcpy(&diffuse, p + 16, sizeof diffuse);
    }
    if (!torchReady()) return false;
    if (kind == objects::Kind::TorchGlow) return true;
    torch::Flame f;
    if (torchFlames.size() >= kMaxTorches || !torch::flameOf(q, f) || f.rhw <= 0.0f) return false;
    f.alpha = std::min(1.0f, float(diffuse >> 24) / 240.0f);  // the .act shows the flame at 240
    torchFlames.push_back(f);
    if (viewToWorldValid && !mapWorld.axes().known()) {  // 0.25: where it stands, for the map file's axes
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float vz = 1.0f / f.rhw, vx = (f.baseX - p.cx) * vz / p.f0, vy = (f.baseY - p.cy) * vz / p.f1;
        const float* m = viewToWorld;
        mapWorld.noteTorch(map::Point{vx * m[0] + vy * m[3] + vz * m[6] + m[9],
                                      vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                                      vx * m[2] + vy * m[5] + vz * m[8] + m[11]});
    }
    if (!torchesLogged) LOG_INFO("torches: the map's torches drawn anew");
    torchesLogged = true;
    return effects.torchFlame;  // 0.23.2: off - the game's own flame, the new glow and light around it
}

// The shaders, the pipelines for this target and the copies of the picture: false when any is missing (logged once).
bool Renderer::Impl::torchReady() {
    if (!torchLightPs || !torchFirePs || !open) return false;
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    const UINT fire = upscale ? 4 : code;
    if (!torchLightPipelines[code]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, torchLightPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&torchLightPipelines[code])))) {
            torchLightPipelines[code] = nullptr;
            LOG_ERROR("torches: the light's pipeline onto %u samples failed: they stay the game's", 1u << code);
            releaseCom(torchLightPs);  // not tried again
            return false;
        }
    }
    if (!torchFirePipelines[fire]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, torchFirePs, DXGI_FORMAT_B8G8R8A8_UNORM, upscale ? 1 : 1u << code);
        D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
        b.BlendEnable = TRUE;  // premultiplied: the glow adds light, the flame hides some of what is behind it
        b.SrcBlend = D3D12_BLEND_ONE;
        b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOp = D3D12_BLEND_OP_ADD;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;  // under DLSS the world's alpha is its mask (0.22.3): the flame marks it
        b.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        b.LogicOp = D3D12_LOGIC_OP_NOOP;
        if (!upscale)
            b.RenderTargetWriteMask =
                D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&torchFirePipelines[fire])))) {
            torchFirePipelines[fire] = nullptr;
            LOG_ERROR("torches: the fire's pipeline failed: they stay the game's");
            releaseCom(torchFirePs);
            return false;
        }
    }
    if (!torchData) {  // a texel a torch for the light's pass
        torchData = createTexture2D(device, int(kMaxLights * 2), 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                    D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        torchDataSrv = torchData ? allocSrv() : kNoSrv;
        if (!torchData || torchDataSrv == kNoSrv) {
            LOG_ERROR("torches: no memory for their list: they stay the game's");
            releaseCom(torchData);
            releaseCom(torchLightPs);
            return false;
        }
        torchDataState = D3D12_RESOURCE_STATE_COPY_DEST;
        device->CreateShaderResourceView(torchData, nullptr, cpuSrvHandle(torchDataSrv));
    }
    return ensureScene("torches");
}

namespace {

// A light of the light's pass (torch_light_ps, two texels): its place in the view, strength, colour and reach.
struct LightItem {
    float view[3];
    float strength;
    float color[3];
    float reach;  // world units
};

// 0.25: the map file's torches the game does not show (it knows effect ids up to 629): their colours.
bool hiddenTorch(int32_t id, float tint[3]) {
    static const float kRed[3] = {1.0f, 0.32f, 0.26f}, kGreen[3] = {0.42f, 1.0f, 0.38f};
    static const float kPurple[3] = {0.78f, 0.4f, 1.0f};
    const float* c = id == 690 ? kRed : id == 691 ? kGreen : id == 696 ? kPurple : nullptr;
    if (!c) return false;
    std::copy(c, c + 3, tint);
    return true;
}

// A place of the game's world in the view (the camera backwards, inverted: its rows are orthonormal).
map::Point toView(const float m[12], const map::Point& w) {
    const float dx = w.x - m[9], dy = w.y - m[10], dz = w.z - m[11];
    return map::Point{dx * m[0] + dy * m[1] + dz * m[2], dx * m[3] + dy * m[4] + dz * m[5],
                      dx * m[6] + dy * m[7] + dz * m[8]};
}

}  // namespace

// Once the world is drawn: the light of this frame's flames and lamps into the picture (one pass, from its copy),
// then each flame and the glow around it (a pass each, cut to it). 0.25: with the map file read and its axes known,
// the torches the game does not show and the lamps' lights join those the game draws.
bool Renderer::Impl::applyTorches() {
    torchDone = true;  // the world's draws after this are the game's own
    mapWorld.poll();
    std::vector<torch::Flame> flames;
    flames.swap(torchFlames);
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float time = waterTime();  // the effects' clock
    const float amount = 0.35f * float(std::clamp(effects.torchFlicker, 0, 100)) / 100.0f;
    const rsw::World* layout = viewToWorldValid && mapWorld.axes().known() ? mapWorld.world() : nullptr;
    const map::Axes axes = mapWorld.axes();
    // Off the screen by more than this (game pixels) a flame or a lamp's middle is not drawn nor lit from.
    const float margin = float(std::max(gameWidth, gameHeight)) * 0.25f;
    const auto onScreen = [&](float gx, float gy, float slack) {
        return gx > -slack && gy > -slack && gx < float(gameWidth) + slack && gy < float(gameHeight) + slack;
    };
    if (layout && torchWanted())
        for (const rsw::Effect& e : layout->effects) {
            torch::Flame f;
            if (flames.size() >= kMaxTorches || !hiddenTorch(e.id, f.tint)) continue;
            const map::Point base = map::toWorld(axes, map::Point{e.pos[0], e.pos[1] - 2.0f, e.pos[2]});
            const map::Point vb = toView(viewToWorld, base);
            const map::Point vt = toView(viewToWorld, map::Point{base.x, base.y - 13.0f, base.z});  // 13 units tall
            if (vb.z <= proj.nearZ || vt.z <= proj.nearZ) continue;
            f.baseX = vb.x / vb.z * proj.f0 + proj.cx;
            f.baseY = vb.y / vb.z * proj.f1 + proj.cy;
            f.upX = vt.x / vt.z * proj.f0 + proj.cx - f.baseX;
            f.upY = vt.y / vt.z * proj.f1 + proj.cy - f.baseY;
            f.halfWidth = std::hypot(f.upX, f.upY) * (torch::kHalfWidth / torch::kHeight);
            f.rhw = 1.0f / vb.z;
            if (onScreen(f.baseX, f.baseY, margin)) flames.push_back(f);
        }
    // 0.23.3: the shader bounds it. 0.25.2: four fifths of 0.25's at 100 % ("too much and too dense" once the torch
    // has come up from its fade); 0.25.3: a little less again ("ещё чуточку"); 0.25.4: near half ("in the original
    // not so bright" - the game's torch lights nothing, its glow is baked into the ground).
    const float strength = 0.4f * float(std::clamp(effects.torchLight, 0, 200)) / 100.0f;
    std::vector<LightItem> lights;
    std::vector<float> flickers(flames.size(), 1.0f), seeds(flames.size(), 0.0f);
    for (size_t i = 0; i < flames.size(); ++i) {
        const torch::Flame& f = flames[i];
        const float mx = f.baseX + f.upX * kMiddle, my = f.baseY + f.upY * kMiddle;  // game pixels
        const float vz = 1.0f / f.rhw;
        const float vx = (mx - proj.cx) * vz / proj.f0, vy = (my - proj.cy) * vz / proj.f1;
        uint32_t s = 0;
        if (viewToWorldValid) {  // its place on the map: its own flicker whichever way the camera looks
            const float* m = viewToWorld;
            s = torch::seedAt(vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[2] + vy * m[5] + vz * m[8] + m[11]);
        } else {
            s = torch::seedAt(mx, my);
        }
        flickers[i] = torch::flicker(time, s, amount);
        seeds[i] = float(s % 1000u) / 1000.0f;
        if (strength <= 0.0f) continue;
        // 0.25.1: the light on the ground flickers less and slower than the flame ("мигает слишком сильно"): a
        // third of its swing at half its pace.
        const float calm = torch::flicker(double(time) * 0.5, s ^ 0x5BD1E995u, amount * 0.35f);
        LightItem l{{vx, vy, vz}, strength * calm * f.alpha, {}, kReach};
        const bool hidden = f.tint[0] < 0.99f || f.tint[1] < 0.99f || f.tint[2] < 0.99f;  // its own colour
        std::copy(hidden ? f.tint : kLightColor, (hidden ? f.tint : kLightColor) + 3, l.color);
        lights.push_back(l);
    }
    // 0.25: the lamps the map's makers placed, the nearest to the camera first, as many as the pass takes.
    if (layout && lampsWanted() && effects.lampsLight > 0) {
        const float lampStrength = 0.8f * float(std::clamp(effects.lampsLight, 0, 200)) / 100.0f;
        std::vector<LightItem> lamps;
        for (const rsw::Light& r : mapWorld.lamps()) {
            const map::Point w = map::toWorld(axes, map::Point{r.pos[0], r.pos[1], r.pos[2]});
            const map::Point v = toView(viewToWorld, w);
            const float reach = std::clamp(r.range * 0.45f, 15.0f, 70.0f);
            if (v.z <= proj.nearZ - reach) continue;
            const float vz = std::max(v.z, proj.nearZ);
            const float gx = v.x / vz * proj.f0 + proj.cx, gy = v.y / vz * proj.f1 + proj.cy;
            if (!onScreen(gx, gy, reach * proj.f0 / vz + margin)) continue;
            const float top = std::max({r.color[0], r.color[1], r.color[2], 1e-3f});
            LightItem l{{v.x, v.y, v.z}, lampStrength, {r.color[0] / top, r.color[1] / top, r.color[2] / top}, reach};
            // A fire's colour - red the most, then green, then blue - flickers, each its own way.
            if (r.color[0] >= r.color[1] && r.color[1] >= r.color[2] * 0.9f && r.color[0] > 0.5f)
                l.strength *= torch::flicker(double(time) * 0.5, torch::seedAt(w.x, w.z), amount * 0.25f);
            lamps.push_back(l);
        }
        std::sort(lamps.begin(), lamps.end(),
                  [](const LightItem& a, const LightItem& b) { return a.view[2] < b.view[2]; });
        for (const LightItem& l : lamps) {
            if (lights.size() >= kMaxLights) break;
            lights.push_back(l);
        }
    }
    if (lights.size() > kMaxLights) lights.resize(kMaxLights);
    if ((flames.empty() && lights.empty()) || !torchReady()) return true;
    if (!copySceneColor() || !copySceneDepth()) return false;
    const float k = float(upscale ? worldScale : scale);
    const float jx = upscale ? jitter[0] : 0.0f, jy = upscale ? jitter[1] : 0.0f;
    const int w = sceneWidth, h = sceneHeight;
    const float glowStrength = float(std::clamp(effects.torchGlow, 0, 200)) / 100.0f;
    // 0.23.2: the flame softer than 0.23's at 100 % ("too harsh"), none where the game's own is drawn - the torches
    // the game does not show always have the new one (0.25).
    const float brightness = 0.65f * float(std::clamp(effects.torchBrightness, 0, 200)) / 100.0f;  // 0.25.4: was 0.8
    ID3D12Resource* dst = upscale ? world : target;
    D3D12_RESOURCE_STATES& dstState = upscale ? worldState : targetState;
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(upscale ? kRtvWorld : kRtvTarget);
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = k;
    c.jitter[0] = jx;
    c.jitter[1] = jy;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    if (!lights.empty()) {  // the list to the GPU: two texels a light
        float data[kMaxLights * 2][4] = {};
        float lx0 = float(w), ly0 = float(h), lx1 = 0.0f, ly1 = 0.0f;  // where the light falls on the screen
        for (size_t i = 0; i < lights.size(); ++i) {
            const LightItem& l = lights[i];
            std::copy(l.view, l.view + 3, data[2 * i]);
            data[2 * i][3] = l.strength;
            std::copy(l.color, l.color + 3, data[2 * i + 1]);
            data[2 * i + 1][3] = l.reach;
            const float vz = std::max(l.view[2], proj.nearZ);
            const float r = l.reach * proj.f0 / vz * k;  // the reach on the screen at its depth
            const float tx = (l.view[0] / vz * proj.f0 + proj.cx) * k + 0.5f + jx;
            const float ty = (l.view[1] / vz * proj.f1 + proj.cy) * k + 0.5f + jy;
            lx0 = std::min(lx0, tx - r);
            ly0 = std::min(ly0, ty - r);
            lx1 = std::max(lx1, tx + r);
            ly1 = std::max(ly1, ty + r);
        }
        const Upload u = allocUpload(sizeof data, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (!u.cpu) return true;  // no upload memory this frame: the torches wait for the next
        std::memcpy(u.cpu, data, sizeof data);
        D3D12_TEXTURE_COPY_LOCATION src{}, to{};
        src.pResource = slots[slot].upload;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = u.offset;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        src.PlacedFootprint.Footprint.Width = kMaxLights * 2;
        src.PlacedFootprint.Footprint.Height = 1;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = UINT(sizeof data);  // 2048: a multiple of 256
        to.pResource = torchData;
        to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        transition(torchData, torchDataState, D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
        transition(torchData, torchDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        c.footprint = float(lights.size());
        const D3D12_RECT lit = clipped(lx0, ly0, lx1, ly1, w, h);
        if (lit.right > lit.left && lit.bottom > lit.top &&
            !effectPass(torchLightPipelines[code], dst, dstState, rtv, w, h, c, sceneColorSrv, true, torchDataSrv,
                        &lit))
            return false;
    }
    for (size_t i = 0; i < flames.size(); ++i) {
        const torch::Flame& f = flames[i];
        const float bx = f.baseX * k + 0.5f + jx, by = f.baseY * k + 0.5f + jy;
        const float ux = f.upX * k, uy = f.upY * k;
        const float fh = std::hypot(ux, uy);
        if (fh < 2.0f) continue;  // too small to see
        const bool hidden = f.tint[0] < 0.99f || f.tint[1] < 0.99f || f.tint[2] < 0.99f;
        c.sourceRect[0] = bx;
        c.sourceRect[1] = by;
        c.sourceRect[2] = ux;
        c.sourceRect[3] = uy;
        c.sourceClamp[0] = f.halfWidth * k;
        c.sourceClamp[1] = 1.0f / f.rhw;
        c.sourceClamp[3] = seeds[i];
        c.uiPixels[2] = flickers[i];
        c.uiPixels[3] = time;
        c.twin[0] = glowStrength;
        c.twin[1] = f.alpha;
        c.twin[2] = effects.torchFlame || hidden ? brightness : 0.0f;
        std::copy(f.tint, f.tint + 3, c.fogColor);
        const float mx = bx + ux * kMiddle, my = by + uy * kMiddle, r = fh * kGlowRadius;
        const D3D12_RECT around = clipped(mx - r, my - r, mx + r, my + r, w, h);
        if (around.right <= around.left || around.bottom <= around.top) continue;
        if (!effectPass(torchFirePipelines[upscale ? 4 : code], dst, dstState, rtv, w, h, c, sceneColorSrv, true,
                        kNoSrv, &around))
            return false;
    }
    return true;
}

}  // namespace uagfx::gpu
