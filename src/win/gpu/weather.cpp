#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.39: blended over the target (its samples) or the world under DLSS (one sample) by its alpha.
ID3D12PipelineState* Renderer::Impl::weatherPipeline(UINT n) {
    return alphaPipeline(weatherPs, weatherPipelines, n, "weather");
}

// Blended over by its alpha, RGB only (the weather, 0.64 the rain's splashes); null, and ps dropped, when it fails.
ID3D12PipelineState* Renderer::Impl::alphaPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[4], UINT n,
                                                   const char* what) {
    return blendPipeline(ps, pipelines, n, what, 0);
}

// 0.73: added on, RGB only (the light in the air)
ID3D12PipelineState* Renderer::Impl::addPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[4], UINT n,
                                                 const char* what) {
    return blendPipeline(ps, pipelines, n, what, 1);
}

// 0.77: multiplied onto the picture, RGB only (the sun's shadows)
ID3D12PipelineState* Renderer::Impl::mulPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[4], UINT n,
                                                 const char* what) {
    return blendPipeline(ps, pipelines, n, what, 2);
}

ID3D12PipelineState* Renderer::Impl::blendPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[4], UINT n,
                                                   const char* what, int mode) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (pipelines[code] || !ps) return pipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, ps, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = mode == 1 || mode == 3 ? D3D12_BLEND_ONE : mode == 2 ? D3D12_BLEND_ZERO : D3D12_BLEND_SRC_ALPHA;
    b.DestBlend = mode == 1 ? D3D12_BLEND_ONE : mode == 2 ? D3D12_BLEND_SRC_COLOR : D3D12_BLEND_INV_SRC_ALPHA;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&pipelines[code])))) {
        LOG_ERROR("%s: the pipeline onto %u samples failed: none", what, 1u << code);
        pipelines[code] = nullptr;
        releaseCom(ps);  // not tried again
    }
    return pipelines[code];
}

// The world drawn and finished: the map's weather over it, in three layers from far to near - each moved by the
// camera's own moves a little (the near one the most), so that it hangs in the air rather than on the screen.
bool Renderer::Impl::applyWeather() {
    weatherDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeWeather);  // 0.39.2
    if (!ensureScene("weather")) return true;
    ID3D12PipelineState* pso = weatherPipeline(upscale ? 1 : samples);
    if (!pso) return true;
    const int w = sceneWidth, h = sceneHeight;
    Constants c{};
    c.scale = float(upscale ? worldScale : scale);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.twin[0] = float(int(mapWeather));
    c.footprint = float(std::clamp(effects.weatherAmount, 0, 200)) / 100.0f;
    // 0.70: no weather of the map's own - motes, weather_ps's kinds 5 (pollen) and 6 (dust); the pollen fewer at night
    const bool motes = !mapWeatherWanted();
    if (motes) {
        const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
        c.twin[0] = mapOutdoors ? 5.0f : 6.0f;
        c.footprint = float(std::clamp(effects.motesAmount, 0, 200)) / 100.0f;
        if (mapOutdoors) c.footprint *= 1.0f - 0.7f * night;
    }
    c.sharpen = waterTime();
    if (viewToWorldValid) {  // the camera's place along the view's own across and down, in game pixels nearby
        const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
        const float* m = viewToWorld;
        const float perUnit = proj.f0 / 250.0f;
        c.sourceRect[0] = (m[9] * m[0] + m[10] * m[1] + m[11] * m[2]) * perUnit;
        c.sourceRect[1] = (m[9] * m[3] + m[10] * m[4] + m[11] * m[5]) * perUnit;
    }
    if (windShared()) {  // 1.22: blown along the screen by the shared wind
        c.sourceRect[2] = weatherDrift.x;
        c.sourceRect[3] = windScreenX * windNow.strength * 60.0f;
    }
    if (motes && !mapOutdoors && effects.litDust) {  // 1.23: the dust shows in the light (weather_ps)
        if (!copySceneColor()) return false;
        c.twin[1] = 1.0f;
    }
    if (!weatherLogged)
        LOG_INFO("weather: %s over %s", motes ? (mapOutdoors ? "pollen" : "dust") : weatherName(mapWeather),
                 mapName.c_str());
    weatherLogged = true;
    if (!(worldPass(pso, w, h, c, sceneColorSrv, false)))
        return false;
    // 0.64: the rain's splashes on the ground, where it is known (the camera read)
    ID3D12PipelineState* splash = alphaPipeline(splashPs, splashPipelines, upscale ? 1 : samples, "rain's splashes");
    if (motes || mapWeather != weather::Kind::Rain || !effects.rainSplashes || !viewToWorldValid || !splash)
        return true;  // 2.4: the splashes a box of their own
    if (!copySceneDepth()) return false;
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    return worldPass(splash, w, h, c, sceneColorSrv, true);
}

const char* weatherName(weather::Kind k) {
    switch (k) {
    case weather::Kind::Snow: return "snow";
    case weather::Kind::Rain: return "rain";
    case weather::Kind::Leaves: return "leaves";
    case weather::Kind::Sand: return "sand";
    default: return "none";
    }
}

// 1.22: the shared wind now (core/wind.hpp) and what it has carried since the last frame: the weather along the
// screen (pixels).
void Renderer::Impl::updateWind() {
    const float now = waterTime();
    float dt = windClock < 0.0f ? 0.0f : now - windClock;
    if (dt < 0.0f) dt = 0.0f;  // the clock went round its hour
    windClock = now;
    windNow = wind::at(double(now));
    const float* m = viewToWorld;
    const float axis = std::sqrt(m[0] * m[0] + m[2] * m[2]);
    windScreenX = axis > 1e-4f ? (windNow.x * m[0] + windNow.z * m[2]) / axis : 0.0f;
    const float s = windNow.strength;
    weatherDrift.advance(windScreenX * s * 60.0f, 0.0f, dt);
}

}  // namespace uagfx::gpu
