#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.39: blended over the target (its samples) or the world under DLSS (one sample) by its alpha.
ID3D12PipelineState* Renderer::Impl::weatherPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (weatherPipelines[code] || !weatherPs) return weatherPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, weatherPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&weatherPipelines[code])))) {
        LOG_ERROR("weather: the pipeline onto %u samples failed: none", 1u << code);
        weatherPipelines[code] = nullptr;
        releaseCom(weatherPs);  // not tried again
    }
    return weatherPipelines[code];
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
    c.sharpen = waterTime();
    if (viewToWorldValid) {  // the camera's place along the view's own across and down, in game pixels nearby
        const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
        const float* m = viewToWorld;
        const float perUnit = proj.f0 / 250.0f;
        c.sourceRect[0] = (m[9] * m[0] + m[10] * m[1] + m[11] * m[2]) * perUnit;
        c.sourceRect[1] = (m[9] * m[3] + m[10] * m[4] + m[11] * m[5]) * perUnit;
    }
    if (!weatherLogged) LOG_INFO("weather: %s over %s", weatherName(mapWeather), mapName.c_str());
    weatherLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, false)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, false);
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

}  // namespace uagfx::gpu
