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
                                                   const char* what, int mode, ID3DBlob* vs) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (pipelines[code] || !ps) return pipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, vs ? vs : presentVs, ps, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = mode == 1 || mode == 3 ? D3D12_BLEND_ONE : mode == 2 ? D3D12_BLEND_ZERO : D3D12_BLEND_SRC_ALPHA;
    b.DestBlend = mode == 1 ? D3D12_BLEND_ONE : mode == 2 ? D3D12_BLEND_SRC_COLOR : D3D12_BLEND_INV_SRC_ALPHA;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = mode == 4 ? D3D12_BLEND_ONE : D3D12_BLEND_ZERO;  // 3.0.20-beta.26: 4 its coverage laid in
    b.DestBlendAlpha = mode == 4 ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        mode == 4 ? D3D12_COLOR_WRITE_ENABLE_ALL
                  : D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
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
    // 3.0.5-beta.19: the particles as small quads, each layer's cells over the picture (weather_vs) - sand over the
    // whole picture as before (its air)
    const int kind = int(c.twin[0] + 0.5f);
    ID3D12PipelineState* quads = kind == int(weather::Kind::Sand) || !weatherVs
                                     ? nullptr
                                     : blendPipeline(weatherQuadPs, weatherQuadPipelines, upscale ? 1 : samples,
                                                     "weather's particles", 0, weatherVs);
    // 3.0.19-beta.40: in the 3D view the particles in the world round the eye (core/weather3d) - sand's air as it is
    const weather3d::Field field = weather3d::fieldOf(kind, c.footprint);
    const bool inWorld = camera::wideView() && effects.weather3d && viewToWorldValid && field.count > 0 &&
                         weather3dVs && weather3dPs;
    const weather::QuadCells cells = weather::quadCells(kind, float(w) / c.scale, float(h) / c.scale);
    const uint32_t particles = cells.count[0] + cells.count[1] + cells.count[2];
    if (!weatherLogged)
        LOG_INFO("weather: %s over %s%s", motes ? (mapOutdoors ? "pollen" : "dust") : weatherName(mapWeather),
                 mapName.c_str(),
                 quads ? (", " + std::to_string(particles) + " cells' particles as quads").c_str() : "");
    weatherLogged = true;
    if (inWorld) {
        if (!applyWeather3d(kind, field)) return false;
    } else if (quads) {
        c.reproject[0] = float(cells.count[0]);
        c.reproject[1] = float(cells.count[0] + cells.count[1]);
        for (int layer = 0; layer < 3; ++layer) c.reproject[2 + layer] = float(cells.across[layer]);
        if (!worldPass(quads, w, h, c, sceneColorSrv, false, kNoSrv, nullptr, 6 * particles)) return false;
    } else if (!(worldPass(pso, w, h, c, sceneColorSrv, false))) {
        return false;
    }
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

// 3.0.19-beta.40 (the owner's «делаем все по порядку»: in the 3D view the weather fell as flat layers over the
// picture): the 3D view's weather - particles in the world round the eye (weather3d_vs: a box the world's own they
// repeat in, falling, blown by the shared wind; a quad each, the rain's a streak), hidden behind what is nearer (the
// depth's copy), blended over by their alpha; the dust lit by the picture's copy under it
bool Renderer::Impl::applyWeather3d(int kind, const weather3d::Field& f) {
    ID3D12PipelineState* pso =
        blendPipeline(weather3dPs, weather3dPipelines, upscale ? 1 : samples, "3D weather", 0, weather3dVs);
    if (!pso) return true;
    if (!copySceneDepth() || !copySceneColor()) return false;
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
    c.twin[0] = float(kind);
    c.twin[1] = f.side;
    c.twin[2] = f.high;
    c.twin[3] = f.half;
    c.sourceRect[0] = weather3dDrift[0] * f.blown;
    c.sourceRect[1] = weather3dDrift[1] * f.blown;
    c.sourceRect[2] = f.fall;
    c.sourceRect[3] = f.streak;
    if (windShared()) {  // the rain's slant: the wind's way, units a second
        c.fogColor[0] = windNow.x * windNow.strength * weather3d::kWindUnits * f.blown;
        c.fogColor[1] = windNow.z * windNow.strength * weather3d::kWindUnits * f.blown;
    }
    c.footprint = 1.0f;
    c.sharpen = waterTime();
    if (weather3dLogged != mapName) {
        weather3dLogged = mapName;
        LOG_INFO("3D weather: kind %d over %s - %u particles in the world round the eye (a box %.0f wide, %.0f high)",
                 kind, mapName.c_str(), f.count, double(f.side), double(f.high));
    }
    return worldPass(pso, w, h, c, sceneColorSrv, true, kNoSrv, nullptr, f.vertices());
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
    if (windShared()) {  // 3.0.19-beta.40: the 3D view's weather blown in the world
        const float k = s * weather3d::kWindUnits * dt;
        weather3dDrift[0] = std::fmod(weather3dDrift[0] + windNow.x * k, weather3d::kDriftRound);
        weather3dDrift[1] = std::fmod(weather3dDrift[1] + windNow.z * k, weather3d::kDriftRound);
    }
}

}  // namespace uagfx::gpu
