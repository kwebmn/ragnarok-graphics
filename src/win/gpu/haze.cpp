#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.34: blended over the target (its samples) or the world under DLSS (one sample) by its alpha.
ID3D12PipelineState* Renderer::Impl::mistPipeline(UINT n) {
    return alphaPipeline(mistPs, mistPipelines, n, "mists");
}

// Once the world is drawn and lit (before its colours): the low mists - the morning's over the fields and the towns,
// a swamp's (2.6.8: the far haze and the dungeons' dark and low fog, the pass's own until now, gone at the player's
// word; it was the mists' switch too).
bool Renderer::Impl::applyMist() {
    mistDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeMist);  // 0.39.2
    // 1.7: a low fog in the morning over the fields and the towns (morning_fog), a swamp's mist always (swamp)
    const float morning = effects.morningFog && mapOutdoors && !mapDesert ? living::morningFog(minuteNow()) : 0.0f;
    const bool swampy = effects.swamp && mapPlace.swamp;
    if (morning <= 0.0f && !swampy) return true;
    if (!ensureScene("mists")) return true;
    ID3D12PipelineState* pso = mistPipeline(upscale ? 1 : samples);
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    // 2.2.2: the low fog lies at the ground's height at the character (sourceClamp.x, .y 1 when known) - five depths
    // across the screen gave it until now, and walking past walls (their tops, the void beyond) it jumped up and
    // down: the walls went light and dark by turns (alde_dun03)
    if (cameraValid && cameraTargetZ > 0.0f) {
        c.sourceClamp[0] = camera::targetHeight(viewToWorld, cameraTargetZ);
        c.sourceClamp[1] = 1.0f;
    }
    if (swampy) {  // 1.7: a swamp's mist, always
        static const float kMurk[3] = {0.62f, 0.68f, 0.55f};
        std::copy(kMurk, kMurk + 3, c.sourceRect);
        c.sourceRect[3] = 0.3f;
    }
    if (morning * 0.6f > c.sourceRect[3]) {  // the morning's, thicker
        static const float kMist[3] = {0.84f, 0.87f, 0.9f};
        std::copy(kMist, kMist + 3, c.sourceRect);
        c.sourceRect[3] = std::min(1.0f, morning * 0.6f);
    }
    c.sharpen = waterTime();
    if (!mistLogged) LOG_INFO("mists: %s over %s", swampy ? "a swamp's" : "the morning's", mapName.c_str());
    mistLogged = true;
    return worldPass(pso, w, h, c, sceneDepthSrv, true);
}

// 0.73: the light in the air - about the torches' pass's lights (their list left in torchData) and, under the open
// sky, rays from where the sky shows past the map's edge (shafts_ps), added on.
bool Renderer::Impl::applyShafts() {
    shaftsDone = true;
    EffectTime timing(*this, kTimeShafts);
    if (!ensureScene("light in the air") || !torchReady() || !shaftsPs || !halfOverPs) return true;
    // 2.5.7: at half the world's size into a target of its own, then added over the world smoothly
    if (!shaftsHalfPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, shaftsPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&shaftsHalfPipeline)))) {
            shaftsHalfPipeline = nullptr;
            LOG_ERROR("light in the air: the pipeline failed: none");
            releaseCom(shaftsPs);  // not tried again
            return true;
        }
    }
    ID3D12PipelineState* pso = addPipeline(halfOverPs, halfAddPipelines, upscale ? 1 : samples, "light in the air");
    if (!pso) return true;
    const int hw = std::max(1, (sceneWidth + 1) / 2), hh = std::max(1, (sceneHeight + 1) / 2);
    if (!shaftsTarget || hw != shaftsWidth || hh != shaftsHeight) {
        discard(shaftsTarget);
        shaftsTarget = createTexture2D(device, hw, hh, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                       D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                       nullptr);
        shaftsState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        shaftsWidth = hw;
        shaftsHeight = hh;
        if (!shaftsTarget) {
            LOG_ERROR("light in the air: no video memory at %dx%d: none", hw, hh);
            releaseCom(shaftsPs);
            return true;
        }
        nameResource(shaftsTarget, "light in the air");
        device->CreateRenderTargetView(shaftsTarget, nullptr, rtvHandle(kRtvShafts));
        device->CreateShaderResourceView(shaftsTarget, nullptr, cpuSrvHandle(shaftsSrv));
    }
    if (!copySceneDepth()) return false;
    transition(torchData, torchDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const int w = sceneWidth, h = sceneHeight;
    const float user = float(std::clamp(effects.shaftsStrength, 0, 200)) / 100.0f;
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = float(shaftLights);
    c.twin[0] = 0.22f * user * (1.0f + 0.6f * night);  // the glow shows more in the dark
    c.twin[1] = mapOutdoors && effects.skyRays ? 0.5f * user * (1.0f - night) : 0.0f;  // 2.4: a box of their own
    // The sun up and to the left past the screen - against the way the characters' shadows fall (actor_shadows.cpp)
    float sun[2];
    sunOnScreen(sun);
    c.sourceRect[0] = float(w) * (0.5f - sun[0] * 1.6f);
    c.sourceRect[1] = float(h) * (0.5f - sun[1] * 1.6f);
    c.sourceRect[2] = 1.0f;
    c.sourceRect[3] = 2.0f;  // 2.5.7: a pixel for the world's two by two
    c.twin[2] = float(skillAirFirst);  // 2.7.36: the skills' lights, a third of the glow
    c.twin[3] = float(skillAirEnd);
    const UINT lights = torchDataSrv;
    transition(shaftsTarget, shaftsState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    list->ClearRenderTargetView(rtvHandle(kRtvShafts), none, 0, nullptr);
    if (!effectPass(shaftsHalfPipeline, shaftsTarget, shaftsState, rtvHandle(kRtvShafts), hw, hh, c, sceneColorSrv,
                    true, lights))
        return false;
    transition(shaftsTarget, shaftsState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return worldPass(pso, w, h, c, shaftsSrv, false);
}

// 0.72: the floors reflect what stands on them - indoors and in the rain (floor_reflect_ps, from the picture's copy).
bool Renderer::Impl::applyFloorReflections() {
    floorReflectDone = true;
    EffectTime timing(*this, kTimeFloor);
    if (!ensureScene("floor reflections") || !halfOverPs || !floorReflectPs) return true;
    // 0.86: at half the world's size (a quarter of the rays), then laid over it smoothly.
    if (!floorReflectPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, floorReflectPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&floorReflectPipeline)))) {
            floorReflectPipeline = nullptr;
            releaseCom(floorReflectPs);
            LOG_ERROR("floor reflections: the pipeline failed: none");
            return true;
        }
    }
    ID3D12PipelineState* over =
        blendPipeline(halfOverPs, halfOverPipelines, upscale ? 1 : samples, "floor reflections", 3);
    if (!over) return true;
    const int w = sceneWidth, h = sceneHeight;
    const int hw = std::max(1, (w + 1) / 2), hh = std::max(1, (h + 1) / 2);
    if (!floorTarget || hw != floorWidth || hh != floorHeight) {
        discard(floorTarget);  // 2.4.3
        floorTarget = createTexture2D(device, hw, hh, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                      nullptr);
        floorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        floorWidth = hw;
        floorHeight = hh;
        if (!floorTarget) {
            LOG_ERROR("floor reflections: no video memory at %dx%d", hw, hh);
            releaseCom(floorReflectPs);
            return true;
        }
        device->CreateRenderTargetView(floorTarget, nullptr, rtvHandle(kRtvFloor));
        device->CreateShaderResourceView(floorTarget, nullptr, cpuSrvHandle(floorSrv));
    }
    if (!copySceneColor() || !copySceneDepth()) return false;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.twin[0] = floorReflectAmount();
    c.twin[1] = wetWanted() ? 1.0f : 0.0f;  // 0.97
    c.twin[2] = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;  // 2.7.36: the puddles' sky darker at night
    c.sharpen = waterTime();                // 1.7: the drops' rings in the puddles
    if (!floorReflectLogged) LOG_INFO("floor reflections: on %s, at %dx%d", mapName.c_str(), hw, hh);
    floorReflectLogged = true;
    if (!effectPass(floorReflectPipeline, floorTarget, floorState, rtvHandle(kRtvFloor), hw, hh, c, sceneColorSrv,
                    true))
        return false;
    transition(floorTarget, floorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return worldPass(over, w, h, c, floorSrv, false);
}

}  // namespace uagfx::gpu
