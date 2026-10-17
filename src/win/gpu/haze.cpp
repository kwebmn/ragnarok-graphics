#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.34: blended over the target (its samples) or the world under DLSS (one sample) by its alpha.
ID3D12PipelineState* Renderer::Impl::hazePipeline(UINT n) {
    return alphaPipeline(hazePs, hazePipelines, n, "haze");
}

// Once the world is drawn and lit (before its colours): far off it goes over to the haze, over the dungeons' low
// ground a fog drifts (core/haze.hpp: by the map).
bool Renderer::Impl::applyHaze() {
    hazeDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeHaze);  // 0.39.2
    if (!ensureScene("haze")) return true;
    ID3D12PipelineState* pso = hazePipeline(upscale ? 1 : samples);
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    const float user = float(std::clamp(effects.hazeStrength, 0, 100)) / 50.0f;  // 50 % the look's own
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
    c.twin[0] = mapLook.hazeStart;
    c.twin[1] = mapLook.hazeEnd;
    c.twin[2] = std::min(1.0f, mapLook.haze * user);  // twinW (1.18: until now into twinH - no haze far off)
    std::copy(mapLook.hazeColour, mapLook.hazeColour + 3, c.fogColor);
    std::copy(mapLook.fogColour, mapLook.fogColour + 3, c.sourceRect);
    c.sourceRect[3] = std::min(1.0f, mapLook.fog * user);
    // 1.7: a low fog in the morning over the fields and the towns (morning_fog)
    const float morning = effects.morningFog && mapOutdoors && !mapDesert ? living::morningFog(minuteNow()) : 0.0f;
    if (effects.swamp && mapPlace.swamp && c.sourceRect[3] < 0.3f) {  // 1.7: a swamp's mist, always
        static const float kMurk[3] = {0.62f, 0.68f, 0.55f};
        for (int i = 0; i < 3; ++i) c.sourceRect[i] = kMurk[i];
        c.sourceRect[3] = 0.3f;
    }
    if (morning * 0.6f > c.sourceRect[3]) {
        static const float kMist[3] = {0.84f, 0.87f, 0.9f};
        const float k = mapLook.fog > 0.0f ? 0.5f : 1.0f;  // the map's own fog's colour kept, half
        for (int i = 0; i < 3; ++i) c.sourceRect[i] = c.sourceRect[i] * (1.0f - k) + kMist[i] * k;
        c.sourceRect[3] = std::min(1.0f, morning * 0.6f * std::max(user, 0.5f));
    }
    c.sharpen = waterTime();
    if (!hazeLogged) LOG_INFO("haze: over %s%s", mapName.c_str(), mapLook.fog > 0.0f ? ", with a low fog" : "");
    hazeLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneDepthSrv, true)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneDepthSrv, true);
}

// 0.73: the light in the air - about the torches' pass's lights (their list left in torchData) and, under the open
// sky, rays from where the sky shows past the map's edge (shafts_ps), added on.
bool Renderer::Impl::applyShafts() {
    shaftsDone = true;
    EffectTime timing(*this, kTimeShafts);
    if (!ensureScene("light in the air") || !torchReady()) return true;
    ID3D12PipelineState* pso = addPipeline(shaftsPs, shaftsPipelines, upscale ? 1 : samples, "light in the air");
    if (!pso) return true;
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
    c.twin[1] = mapOutdoors ? 0.5f * user * (1.0f - night) : 0.0f;
    // The sun up and to the left past the screen - against the way the characters' shadows fall (actor_shadows.cpp)
    float sun[2];
    sunOnScreen(sun);
    c.sourceRect[0] = float(w) * (0.5f - sun[0] * 1.6f);
    c.sourceRect[1] = float(h) * (0.5f - sun[1] * 1.6f);
    c.sourceRect[2] = 1.0f;
    const UINT lights = torchDataSrv;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, true, lights)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, true, lights);
}

// 0.98: the sun's flare - where the sky shows towards the sun (up and to the left past the screen, against the way
// the characters' shadows fall), a glow at that edge and a few rings along the line through the middle (flare_ps).
bool Renderer::Impl::applyFlare() {
    flareDone = true;
    if (!ensureScene("sun flare")) return true;
    ID3D12PipelineState* pso = addPipeline(flarePs, flarePipelines, upscale ? 1 : samples, "sun flare");
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    Constants c{};
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    float sun[2];
    sunOnScreen(sun);
    c.sourceRect[0] = float(w) * (0.5f - sun[0] * 1.6f);
    c.sourceRect[1] = float(h) * (0.5f - sun[1] * 1.6f);
    c.twin[0] = 0.35f * (1.0f - night / 0.6f);
    if (!flareLogged) LOG_INFO("sun flare: on %s", mapName.c_str());
    flareLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, true)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, true);
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
        if (floorTarget) slots[slot].garbage.push_back(floorTarget);
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
    c.sharpen = waterTime();                // 1.7: the drops' rings in the puddles
    if (!floorReflectLogged) LOG_INFO("floor reflections: on %s, at %dx%d", mapName.c_str(), hw, hh);
    floorReflectLogged = true;
    if (!effectPass(floorReflectPipeline, floorTarget, floorState, rtvHandle(kRtvFloor), hw, hh, c, sceneColorSrv,
                    true))
        return false;
    transition(floorTarget, floorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return upscale ? effectPass(over, world, worldState, rtvHandle(kRtvWorld), w, h, c, floorSrv, false)
                   : effectPass(over, target, targetState, rtvHandle(kRtvTarget), w, h, c, floorSrv, false);
}

// 0.69: past the map's edge on a map under the open sky - the sky, its clouds, at night its stars (sky_ps).
bool Renderer::Impl::applySky() {
    skyDone = true;
    EffectTime timing(*this, kTimeSky);
    if (!ensureScene("sky")) return true;
    ID3D12PipelineState* pso = alphaPipeline(skyPs, skyPipelines, upscale ? 1 : samples, "sky");
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
    // The horizon in the haze's colour where the map has one (the void it filled goes over into the sky), a pale blue
    // otherwise; the zenith a deeper blue; both darker and bluer at night.
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    const bool hazy = hazeWanted() && mapLook.haze > 0.0f;
    const float pale[3] = {0.72f, 0.80f, 0.91f}, deep[3] = {0.33f, 0.52f, 0.86f}, dark[3] = {0.03f, 0.05f, 0.11f};
    for (int k = 0; k < 3; ++k) {
        const float horizon = hazy ? mapLook.hazeColour[k] : pale[k];
        c.fogColor[k] = horizon + (dark[k] * 2.0f - horizon) * night;
        c.sourceRect[k] = deep[k] + (dark[k] - deep[k]) * night;
    }
    c.twin[0] = float(std::clamp(effects.skyClouds, 0, 100)) / 100.0f;
    c.twin[1] = float(std::clamp(effects.skyStrength, 0, 100)) / 100.0f;
    c.twin[2] = night;  // twinW (1.17.1: until now into twinH - the sky never went dark, no stars)
    // 1.17.1: land past the map's edge, below the horizon (sky_ps): how much, the ground's height at the player
    const bool land = effects.edgeLand && cameraValid && cameraTargetZ > 0.0f;
    if (land) {
        c.twin[3] = 1.0f;
        c.sourceClamp[0] = cameraTargetZ * viewToWorld[7] + viewToWorld[10];
        if (!copySceneColor()) return false;
    }
    c.sharpen = waterTime();
    if (!skyLogged) LOG_INFO("sky: over %s", mapName.c_str());
    skyLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneDepthSrv, true)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneDepthSrv, true);
}

}  // namespace uagfx::gpu
