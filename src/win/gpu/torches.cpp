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

// A world draw of one of the game's torch layers (core/torch.hpp) - known by its texture (Renderer::noteTorchTexture):
// true when it is not to be drawn, the glow not at all, the flame noted for applyTorches. Drawn as the game has it
// when the torches cannot be drawn anew here (no shaders, no copy of the picture) or the frame's are drawn already.
bool Renderer::Impl::takeTorch(const FrameRecord& rec, const DrawCmd& d) {
    const auto it = textures.find(d.texture[0]);
    if (it == textures.end() || it->second.width <= 0 || it->second.height <= 0) return false;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > v.size()) return false;
    torch::Corner q[4];
    float su = 0.0f, sv = 0.0f;
    uint32_t diffuse = 0;
    for (int i = 0; i < 4; ++i) {
        const uint8_t* p = v.data() + d.vertexOffset + size_t(i) * stride;
        float xyzw[4], uv[2];
        std::memcpy(xyzw, p, sizeof xyzw);
        std::memcpy(uv, p + 24, sizeof uv);  // after the position, the diffuse and the specular colours
        q[i] = torch::Corner{xyzw[0], xyzw[1], xyzw[2], xyzw[3], uv[0], uv[1]};
        su += uv[0];
        sv += uv[1];
        if (i == 0) std::memcpy(&diffuse, p + 16, sizeof diffuse);
    }
    const Texture& t = it->second;
    const torch::Layer layer = torchCells.at(d.texture[0], su * 0.25f * float(t.width), sv * 0.25f * float(t.height));
    if (layer == torch::Layer::None || !torchReady()) return false;
    if (layer == torch::Layer::Glow) return true;
    torch::Flame f;
    if (torchFlames.size() >= kMaxTorches || !torch::flameOf(q, f) || f.rhw <= 0.0f) return false;
    f.alpha = std::min(1.0f, float(diffuse >> 24) / 240.0f);  // the .act shows the flame at 240
    torchFlames.push_back(f);
    if (!torchesLogged) LOG_INFO("torches: the map's torches drawn anew");
    torchesLogged = true;
    return true;
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
        torchData = createTexture2D(device, int(kMaxTorches), 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
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

// Once the world is drawn: the light of this frame's flames into the picture (one pass, from its copy), then each
// flame and the glow around it (a pass each, cut to it).
bool Renderer::Impl::applyTorches() {
    torchDone = true;  // the world's draws after this are the game's own
    std::vector<torch::Flame> flames;
    flames.swap(torchFlames);
    if (flames.empty() || !torchReady()) return true;
    if (!copySceneColor() || !copySceneDepth()) return false;
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float k = float(upscale ? worldScale : scale);
    const float jx = upscale ? jitter[0] : 0.0f, jy = upscale ? jitter[1] : 0.0f;
    const int w = sceneWidth, h = sceneHeight;
    const float time = waterTime();  // the effects' clock
    const float amount = 0.35f * float(std::clamp(effects.torchFlicker, 0, 100)) / 100.0f;
    const float strength = 1.25f * float(std::clamp(effects.torchLight, 0, 200)) / 100.0f;
    const float glowStrength = float(std::clamp(effects.torchGlow, 0, 200)) / 100.0f;
    float data[kMaxTorches][4] = {};
    float flickers[kMaxTorches] = {};
    float seed[kMaxTorches] = {};
    float lx0 = float(w), ly0 = float(h), lx1 = 0.0f, ly1 = 0.0f;  // where the light falls on the screen
    const size_t count = std::min(flames.size(), size_t(kMaxTorches));
    for (size_t i = 0; i < count; ++i) {
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
        seed[i] = float(s % 1000u) / 1000.0f;
        data[i][0] = vx;
        data[i][1] = vy;
        data[i][2] = vz;
        data[i][3] = strength * flickers[i] * f.alpha;
        const float r = kReach * proj.f0 / vz * k;  // the reach on the screen at the flame's depth
        const float tx = mx * k + 0.5f + jx, ty = my * k + 0.5f + jy;
        lx0 = std::min(lx0, tx - r);
        ly0 = std::min(ly0, ty - r);
        lx1 = std::max(lx1, tx + r);
        ly1 = std::max(ly1, ty + r);
    }
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
    if (strength > 0.0f) {  // the list to the GPU: a texel a torch
        const Upload u = allocUpload(kMaxTorches * 16, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (!u.cpu) return true;  // no upload memory this frame: the torches wait for the next
        std::memcpy(u.cpu, data, sizeof data);
        D3D12_TEXTURE_COPY_LOCATION src{}, to{};
        src.pResource = slots[slot].upload;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = u.offset;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        src.PlacedFootprint.Footprint.Width = kMaxTorches;
        src.PlacedFootprint.Footprint.Height = 1;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = kMaxTorches * 16;  // 512: a multiple of 256
        to.pResource = torchData;
        to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        transition(torchData, torchDataState, D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
        transition(torchData, torchDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        c.footprint = float(count);
        c.sharpen = kReach;
        std::copy(kLightColor, kLightColor + 3, c.fogColor);
        const D3D12_RECT lit = clipped(lx0, ly0, lx1, ly1, w, h);
        if (lit.right > lit.left && lit.bottom > lit.top &&
            !effectPass(torchLightPipelines[code], dst, dstState, rtv, w, h, c, sceneColorSrv, true, torchDataSrv,
                        &lit))
            return false;
    }
    for (size_t i = 0; i < count; ++i) {
        const torch::Flame& f = flames[i];
        const float bx = f.baseX * k + 0.5f + jx, by = f.baseY * k + 0.5f + jy;
        const float ux = f.upX * k, uy = f.upY * k;
        const float fh = std::hypot(ux, uy);
        if (fh < 2.0f) continue;  // too small to see
        c.sourceRect[0] = bx;
        c.sourceRect[1] = by;
        c.sourceRect[2] = ux;
        c.sourceRect[3] = uy;
        c.sourceClamp[0] = f.halfWidth * k;
        c.sourceClamp[1] = 1.0f / f.rhw;
        c.sourceClamp[3] = seed[i];
        c.uiPixels[2] = flickers[i];
        c.uiPixels[3] = time;
        c.twin[0] = glowStrength;
        c.twin[1] = f.alpha;
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
