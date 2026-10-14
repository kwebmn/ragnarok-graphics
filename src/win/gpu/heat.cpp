#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.35: the picture read aside from its copy, into the target (its samples) or the world under DLSS (one sample).
ID3D12PipelineState* Renderer::Impl::heatPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (heatPipelines[code] || !heatPs) return heatPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, heatPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    p.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&heatPipelines[code])))) {
        LOG_ERROR("heat haze: the pipeline onto %u samples failed: none", 1u << code);
        heatPipelines[code] = nullptr;
        releaseCom(heatPs);  // not tried again
    }
    return heatPipelines[code];
}

// Once the world is lit (after the torches' pass, which gathers the frame's spots of heat: flames, burning skills,
// lava) and hazed: where the hot air rises the picture shimmers; in the desert far off too.
bool Renderer::Impl::applyHeat() {
    heatDone = true;  // tried once a frame, whatever comes of it
    std::vector<HeatSpot> spots;
    spots.swap(heatSpots);
    const float user = float(std::clamp(effects.heatStrength, 0, 200)) / 100.0f;
    const float desertAmount = heatWanted() && mapDesert && viewToWorldValid ? 0.35f * user : 0.0f;
    if (spots.empty() && desertAmount <= 0.0f) return true;
    if (!ensureScene("heat haze")) return true;
    ID3D12PipelineState* pso = heatPipeline(upscale ? 1 : samples);
    if (!pso) return true;
    if (!heatData) {  // a texel a spot
        heatData = createTexture2D(device, int(kMaxHeat), 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                   D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        heatDataSrv = heatData ? allocSrv() : kNoSrv;
        if (!heatData || heatDataSrv == kNoSrv) {
            LOG_ERROR("heat haze: no memory for its list: none");
            releaseCom(heatData);
            releaseCom(heatPs);
            return true;
        }
        heatDataState = D3D12_RESOURCE_STATE_COPY_DEST;
        device->CreateShaderResourceView(heatData, nullptr, cpuSrvHandle(heatDataSrv));
    }
    if (!copySceneColor() || !copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    std::sort(spots.begin(), spots.end(),
              [](const HeatSpot& a, const HeatSpot& b) { return std::fabs(a.strength) > std::fabs(b.strength); });
    if (spots.size() > kMaxHeat) spots.resize(kMaxHeat);
    float data[kMaxHeat][4] = {};
    float x0 = float(w), y0 = float(h), x1 = 0.0f, y1 = 0.0f;
    for (size_t i = 0; i < spots.size(); ++i) {
        const HeatSpot& s = spots[i];
        data[i][0] = s.x;
        data[i][1] = s.y;
        data[i][2] = s.radius;
        data[i][3] = std::clamp(s.strength, -1.5f, 1.5f);  // the player's setting is in it; below 0 a swirl
        x0 = std::min(x0, s.x - s.radius * 1.2f);
        x1 = std::max(x1, s.x + s.radius * 1.2f);
        y0 = std::min(y0, s.y - s.radius * 2.0f);  // the hot air rises
        y1 = std::max(y1, s.y + s.radius * 0.6f);
    }
    const Upload u = allocUpload(sizeof data, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!u.cpu) return true;  // no upload memory this frame
    std::memcpy(u.cpu, data, sizeof data);
    D3D12_TEXTURE_COPY_LOCATION src{}, to{};
    src.pResource = slots[slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    src.PlacedFootprint.Footprint.Width = kMaxHeat;
    src.PlacedFootprint.Footprint.Height = 1;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = UINT(sizeof data);  // 512: a multiple of 256
    to.pResource = heatData;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    transition(heatData, heatDataState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
    transition(heatData, heatDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(upscale ? worldScale : scale);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = float(spots.size());
    c.twin[0] = desertAmount;
    c.sharpen = waterTime();
    D3D12_RECT clip{0, 0, w, h};
    if (desertAmount <= 0.0f)
        clip = D3D12_RECT{LONG(std::max(0.0f, std::floor(x0))), LONG(std::max(0.0f, std::floor(y0))),
                          LONG(std::min(float(w), std::ceil(x1))), LONG(std::min(float(h), std::ceil(y1)))};
    if (clip.right <= clip.left || clip.bottom <= clip.top) return true;
    if (!heatLogged) LOG_INFO("heat haze: %u spots of heat%s", unsigned(spots.size()), mapDesert ? ", the desert" : "");
    heatLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, true, heatDataSrv,
                                &clip)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, true,
                                heatDataSrv, &clip);
}

}  // namespace uagfx::gpu
