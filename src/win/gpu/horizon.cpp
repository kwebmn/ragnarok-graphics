// 3.0.22-beta.11 (the owner 19.11: «я хочу панорамы горизонта в клиенте красивые», «я хочу чтобы ты нарисовал это на
// видеокарте, не генерировал с помощью ИИ готовое»; core/horizon): Direct3D 12's horizon panorama - the region's land
// baked by horizon_bake_ps into a 360° strip a slice of rows a frame, laid over the sky by skyPanorama (haze.cpp
// skyPass binds it)
#include <windows.h>
#include <d3d12.h>

#include <algorithm>

#include "core/effect_groups.hpp"
#include "core/game_camera.hpp"
#include "core/horizon.hpp"
#include "core/region_look.hpp"
#include "core/view3d.hpp"
#include "win/gpu/horizon_constants.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

bool Renderer::Impl::horizonTextures(const horizon::Level& level) {
    if (horizonTex[0] && horizonTex[1] && horizonW == level.width && horizonH == level.height) return true;
    for (ID3D12Resource*& t : horizonTex) {
        discard(t);
        t = nullptr;
    }
    horizonW = horizonH = 0;
    static const char* const kNames[2] = {"the horizon's land", "the horizon's light"};
    for (int k = 0; k < 2; ++k) {
        horizonTex[k] = createTexture2D(device, level.width, level.height, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                                        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        nullptr);
        horizonState[k] = D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (!horizonTex[k]) {
            LOG_ERROR("horizon: no video memory for its strip at %dx%d - the far ranges stay", level.width,
                      level.height);
            return false;
        }
        nameResource(horizonTex[k], kNames[k]);
        device->CreateRenderTargetView(horizonTex[k], nullptr, rtvHandle(kRtvHorizon + UINT(k)));
        device->CreateShaderResourceView(horizonTex[k], nullptr, cpuSrvHandle(horizonSrv[k]));
    }
    horizonW = level.width;
    horizonH = level.height;
    return true;
}

bool Renderer::Impl::horizonBakeSlice(int row0, int row1) {
    if (!horizonBakePipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, horizonBakePs, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
        p.NumRenderTargets = 2;  // the land's colour and coverage, its light's parts
        p.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&horizonBakePipeline)))) {
            horizonBakePipeline = nullptr;
            horizonFailed = true;
            LOG_ERROR("horizon: the bake's pipeline failed - the far ranges stay");
            return true;
        }
    }
    StageSetup linearClamp;  // the bake samples no texture: any pair
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    if (!makeRoom(linearClamp, linearClamp, 0)) return false;
    placeSamples(false);
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp, 0);
    const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(horizonSrv[0], horizonSrv[1]);  // not read
    for (int k = 0; k < 2; ++k) transition(horizonTex[k], horizonState[k], D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2] = {rtvHandle(kRtvHorizon), rtvHandle(kRtvHorizon + 1)};
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(horizonW), float(horizonH), 0.0f, 1.0f};
    const D3D12_RECT rows{0, row0, horizonW, row1};
    list->OMSetRenderTargets(2, rtvs, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &rows);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(horizonBakePipeline);
    Constants c{};
    c.targetSize[0] = float(horizonW);
    c.targetSize[1] = float(horizonH);
    float packed[horizon::kPackedFloats];
    horizon::pack(horizon::climateOf(horizonBake.key.region), horizonBake.key.mapRadius, horizonBake.key.sun,
                  horizonBake.level, row0, row1, packed);
    horizonConstants(packed, c);
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    return true;
}

void Renderer::Impl::horizonFrame() {
    horizonShown = false;
    if (!horizonBakePs || horizonFailed) return;
    if (!horizon::wanted(effects.skyPanorama, camera::wideView(), mapOutdoors, view3d::hungMap(mapName))) return;
    const ground_heights::Ground* g = mapWorld.ground();
    if (!g || g->width <= 0 || g->height <= 0) return;  // the map's ground not read yet: its size not known
    horizon::Key key;
    key.region = region_look::of(mapName, 100).region;
    float toSun[3] = {0.0f, 0.0f, 0.0f};
    if (mapSun(toSun)) std::copy(toSun, toSun + 3, key.sun);
    key.weak = effect_groups::cardLevel(true, adapterMb) <= 1;
    key.mapRadius = 0.5f * float(std::max(g->width, g->height)) * g->zoom;
    const horizon::Level level = horizon::levelFor(key.weak);
    const ULONGLONG now = GetTickCount64();
    const horizon::Slice s = horizon::step(horizonBake, key, level, now);
    if (s.restart && !horizonTextures(level)) {
        horizonFailed = true;
        return;
    }
    if (s.row1 > s.row0 && !horizonBakeSlice(s.row0, s.row1)) return;  // the GPU lost
    if (s.last)
        LOG_INFO("horizon: %s - the %s climate baked (%dx%d, %d steps, %d shadow steps, %d frames, %llu ms)",
                 mapName.c_str(), key.region.empty() ? "default" : key.region.c_str(), level.width, level.height,
                 level.steps, level.shadowSteps, horizonBake.frames,
                 static_cast<unsigned long long>(now - horizonBake.startedAt));
    horizonShown = horizonBake.ready && !horizonFailed;
    horizonFade = horizon::fadeOf(horizonBake, now);
}

}  // namespace uagfx::gpu
