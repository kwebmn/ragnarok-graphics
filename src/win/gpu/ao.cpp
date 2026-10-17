#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/daylight.hpp"
#include "core/game_camera.hpp"
#include "core/lightning.hpp"
#include "core/region_look.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

D3D12_GRAPHICS_PIPELINE_STATE_DESC fullScreenPipeline(ID3D12RootSignature* root, ID3DBlob* vs, ID3DBlob* ps,
                                                      DXGI_FORMAT format, UINT samples) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = format;
    p.SampleDesc.Count = samples;
    return p;
}

bool Renderer::Impl::effectPass(ID3D12PipelineState* pso, ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState,
                                D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, int dstW, int dstH, Constants& c, UINT srv0,
                                bool scene, UINT srv1, const D3D12_RECT* clip) {
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    if (!makeRoom(linearClamp, linearClamp, 0, scene ? 5 : 2)) return false;  // before the pass records anything
    placeSamples(false);
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp, 0);
    const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(srv0, srv1 == kNoSrv ? srv0 : srv1);
    transition(dst, dstState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(dstW), float(dstH), 0.0f, 1.0f};
    const D3D12_RECT whole{0, 0, dstW, dstH};
    list->OMSetRenderTargets(1, &dstRtv, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, clip ? clip : &whole);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(pso);
    c.targetSize[0] = float(dstW);
    c.targetSize[1] = float(dstH);
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
    if (scene)
        list->SetGraphicsRootDescriptorTable(kRootScene, sceneTable(sceneColorSrv, sceneDepthSrv, sceneColorSrv));
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    return true;
}

void Renderer::Impl::releaseAo() {
    Slot& s = slots[slot];  // what the open list recorded may still read them
    for (ID3D12Resource*& r : aoTarget) {
        if (r) s.garbage.push_back(r);
        r = nullptr;
    }
    aoWidth = aoHeight = 0;
}

// The shadows' two half-sized R8 targets and the pipelines; false when they cannot be made (logged once a size).
bool Renderer::Impl::ensureAo(int w, int h) {
    if (!aoPs || !aoBlurPs || !aoApplyPs) return false;  // its shaders did not compile (logged at the start)
    if (!aoPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p = fullScreenPipeline(root, presentVs, aoPs, DXGI_FORMAT_R8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&aoPipeline))))
            aoPipeline = nullptr;
        p = fullScreenPipeline(root, presentVs, aoBlurPs, DXGI_FORMAT_R8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&aoBlurPipeline))))
            aoBlurPipeline = nullptr;
        if (!aoPipeline || !aoBlurPipeline) {
            LOG_ERROR("shadows: their pipelines failed: they stay off");
            return false;
        }
    }
    // 0.39.3: a quarter of a big world's size (4K: 0.65 ms a frame at half) - its blur evens it out.
    const int div = w >= 3000 ? 4 : 2;
    const int aw = std::max(1, (w + div - 1) / div), ah = std::max(1, (h + div - 1) / div);
    if (aoTarget[0] && aw == aoWidth && ah == aoHeight) return true;
    if (aoFailed && aw == aoWidth && ah == aoHeight) return false;
    releaseAo();
    aoWidth = aw;
    aoHeight = ah;
    aoFailed = true;
    D3D12_CLEAR_VALUE open{};
    open.Format = DXGI_FORMAT_R8_UNORM;
    open.Color[0] = 1.0f;
    for (int i = 0; i < 2; ++i) {
        aoTarget[i] = createTexture2D(device, aw, ah, 1, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                      D3D12_RESOURCE_STATE_RENDER_TARGET, &open);
        aoState[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (!aoTarget[i]) {
            LOG_ERROR("shadows: no video memory for them at %dx%d: they stay off at this size", aw, ah);
            releaseAo();
            aoWidth = aw;  // remembered as failed
            aoHeight = ah;
            return false;
        }
        device->CreateRenderTargetView(aoTarget[i], nullptr, rtvHandle(kRtvAo + UINT(i)));
        device->CreateShaderResourceView(aoTarget[i], nullptr, cpuSrvHandle(aoSrv[i]));
    }
    aoFailed = false;
    LOG_INFO("shadows: %dx%d, from the depth of the %dx%d world", aw, ah, w, h);
    return true;
}

// Multiplied into the target (its samples) or the world under DLSS (one sample).
ID3D12PipelineState* Renderer::Impl::aoApplyPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (aoApplyPipelines[code]) return aoApplyPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, aoApplyPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;  // the picture times the shadow
    b.SrcBlend = D3D12_BLEND_ZERO;
    b.DestBlend = D3D12_BLEND_SRC_COLOR;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&aoApplyPipelines[code])))) {
        LOG_ERROR("shadows: the pipeline onto %u samples failed", 1u << code);
        aoApplyPipelines[code] = nullptr;
    }
    return aoApplyPipelines[code];
}

bool Renderer::Impl::applyAo() {
    aoDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeAo);  // 0.39.2
    if (!ensureScene("shadows")) return true;
    const int w = sceneWidth, h = sceneHeight;
    ID3D12PipelineState* apply = aoApplyPipeline(upscale ? 1 : samples);
    if (!ensureAo(w, h) || !apply) return true;
    if (!copySceneDepth()) return false;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[2] = float(aoWidth);
    c.uiPixels[3] = float(aoHeight);
    // A cell of the map is ten units: the shadows reach 1.5 to 9 of them from a corner.
    c.footprint = 1.5f + 7.5f * float(std::clamp(effects.aoSize, 0, 100)) / 100.0f;
    c.sharpen = float(std::clamp(effects.aoStrength, 0, 100)) / 100.0f;
    if (!effectPass(aoPipeline, aoTarget[0], aoState[0], rtvHandle(kRtvAo), aoWidth, aoHeight, c, sceneDepthSrv,
                    true))  // t0 unread: the depth, in its shader state already
        return false;
    for (int i = 0; i < 2; ++i) {  // across, then down
        transition(aoTarget[i], aoState[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        c.sourceRect[0] = i == 0 ? 1.0f : 0.0f;
        c.sourceRect[1] = i == 0 ? 0.0f : 1.0f;
        if (!effectPass(aoBlurPipeline, aoTarget[1 - i], aoState[1 - i], rtvHandle(kRtvAo + UINT(1 - i)), aoWidth,
                        aoHeight, c, aoSrv[i], true))
            return false;
    }
    transition(aoTarget[0], aoState[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return upscale ? effectPass(apply, world, worldState, rtvHandle(kRtvWorld), w, h, c, aoSrv[0], false)
                   : effectPass(apply, target, targetState, rtvHandle(kRtvTarget), w, h, c, aoSrv[0], false);
}

// 0.77: the way to the sun in the view - from the way the shadows fall on the screen (sunOnScreen) along the ground
// (up is -y) and its height (the time of day's, or 40 degrees).
bool Renderer::Impl::applySunShadows() {
    sunShadowDone = true;
    EffectTime timing(*this, kTimeSun);
    if (!ensureScene("sun's shadows")) return true;
    ID3D12PipelineState* pso = mulPipeline(sunShadowPs, sunShadowPipelines, upscale ? 1 : samples, "sun's shadows");
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    const float* m = viewToWorld;
    const auto flat = [](float x, float z, float out[3]) {  // along the ground, a unit vector
        const float len = std::sqrt(x * x + z * z);
        out[0] = len > 1e-4f ? x / len : 0.0f;
        out[1] = 0.0f;
        out[2] = len > 1e-4f ? z / len : 0.0f;
    };
    float right[3], down[3];
    flat(m[0], m[2], right);
    flat(m[3], m[5], down);
    float sun[2];
    sunOnScreen(sun);
    const daylight::Light day = daylight::at(effects.dayCycle ? effects.dayNow : -1);
    const float height = (effects.dayCycle && effects.dayNow >= 0 ? 15.0f + 50.0f / day.length * 0.55f : 40.0f) *
                         3.14159265f / 180.0f;
    float g[3], toSun[3], view[3];
    for (int k = 0; k < 3; ++k) g[k] = sun[0] * right[k] + sun[1] * down[k];
    const float gl = std::sqrt(g[0] * g[0] + g[2] * g[2]);
    if (gl < 1e-4f) return true;
    for (int k = 0; k < 3; ++k) toSun[k] = -g[k] / gl * std::cos(height);
    toSun[1] = -std::sin(height);  // up
    for (int k = 0; k < 3; ++k) view[k] = toSun[0] * m[3 * k] + toSun[1] * m[3 * k + 1] + toSun[2] * m[3 * k + 2];
    const int w = sceneWidth, h = sceneHeight;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    std::copy(view, view + 3, c.twin);
    c.footprint = 90.0f;
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    c.sharpen = 0.45f * float(std::clamp(effects.sunShadowsStrength, 0, 100)) / 100.0f * (1.0f - night);
    if (effects.dapple && copySceneColor()) {  // 1.7: spots of sun under the trees (sun_shadow_ps)
        std::copy(viewToWorld, viewToWorld + 12, c.reproject);
        c.sourceRect[0] = waterTime();
        c.sourceRect[1] = 1.0f;
    }
    if (!sunShadowLogged) LOG_INFO("sun's shadows: over %s", mapName.c_str());
    sunShadowLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneDepthSrv, true)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneDepthSrv, true);
}

// Into the target (its samples) or the world under DLSS (one sample), as it is: the colours are made in the shader.
bool Renderer::Impl::applyGrade() {
    gradeDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeGrade);  // 0.39.2
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (!gradePs || !ensureScene("colour")) return true;
    if (!gradePipelines[code]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, gradePs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&gradePipelines[code])))) {
            gradePipelines[code] = nullptr;
            LOG_ERROR("colour: the pipeline onto %u samples failed", 1u << code);
            return true;
        }
    }
    if (!copySceneColor()) return false;
    Constants c{};
    c.sharpen = float(std::clamp(effects.gradeSaturation, 0, 200)) / 100.0f;
    c.footprint = float(std::clamp(effects.gradeContrast, 50, 150)) / 100.0f;
    c.twin[0] = float(std::clamp(effects.gradeBrightness, 50, 150)) / 100.0f;
    c.twin[1] = float(std::clamp(effects.gradeWarmth, 0, 100) - 50) / 50.0f;
    c.twin[2] = float(std::clamp(effects.gradeVignette, 0, 100)) / 100.0f;
    c.uiPixels[0] = float(sceneWidth);
    c.uiPixels[1] = float(sceneHeight);
    return upscale ? effectPass(gradePipelines[code], world, worldState, rtvHandle(kRtvWorld), sceneWidth,
                                sceneHeight, c, sceneColorSrv, true)
                   : effectPass(gradePipelines[code], target, targetState, rtvHandle(kRtvTarget), sceneWidth,
                                sceneHeight, c, sceneColorSrv, true);
}

// 0.63: the night (night_ps) - as the colours are made, from the picture's copy into it.
bool Renderer::Impl::applyNight() {
    nightDone = true;
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (!nightPs || !ensureScene("night")) return true;
    if (!nightPipelines[code]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, nightPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&nightPipelines[code])))) {
            nightPipelines[code] = nullptr;
            LOG_ERROR("night: the pipeline onto %u samples failed", 1u << code);
            return true;
        }
    }
    if (!copySceneColor()) return false;
    Constants c{};
    c.sharpen = effects.night ? float(std::clamp(effects.nightNow, 0, 100)) / 100.0f : 0.0f;
    const daylight::Light day = daylight::at(effects.dayCycle ? effects.dayNow : -1);  // 0.74
    const region_look::Look look = region_look::of(mapName, effects.regionLooks ? effects.regionLooksStrength : 0);
    for (int k = 0; k < 3; ++k) c.twin[k] = day.tint[k] * look.tint[k];  // 0.79: and the region's
    c.sourceRect[3] = look.saturation - 1.0f;
    c.sourceRect[0] = flashNow();  // 0.80
    c.uiPixels[0] = float(sceneWidth);
    c.uiPixels[1] = float(sceneHeight);
    return upscale ? effectPass(nightPipelines[code], world, worldState, rtvHandle(kRtvWorld), sceneWidth,
                                sceneHeight, c, sceneColorSrv, true)
                   : effectPass(nightPipelines[code], target, targetState, rtvHandle(kRtvTarget), sceneWidth,
                                sceneHeight, c, sceneColorSrv, true);
}

float Renderer::Impl::flashNow() const {
    if (!effects.storms || !effects.weather || mapWeather != weather::Kind::Rain || !nightPs) return 0.0f;
    return lightning::flash(double(const_cast<Impl*>(this)->waterTime()));
}

// 0.81: grass blades from the green ground (grass_ps), trodden down where the last frame's steps are.
bool Renderer::Impl::applyGrass() {
    grassDone = true;
    EffectTime timing(*this, kTimeGrass);
    if (!ensureScene("grass")) return true;
    ID3D12PipelineState* pso = alphaPipeline(grassPs, grassPipelines, upscale ? 1 : samples, "grass");
    if (!pso) return true;
    if (!copySceneColor() || !copySceneDepth()) return false;
    const bool steps = stepsData && !footTracker.steps().empty();
    if (steps) transition(stepsData, stepsDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const int w = sceneWidth, h = sceneHeight;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    const float k = float(upscale ? worldScale : scale);
    c.scale = k;
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = steps ? float(std::min(footTracker.steps().size(), footsteps::Tracker::kMax)) : 0.0f;
    // The longest blade: about 1.6 units at the camera's usual distance.
    c.twin[0] = std::clamp(std::round(1.6f * proj.f0 * k / 350.0f), 4.0f, 28.0f);
    c.twin[1] = 0.65f * float(std::clamp(effects.grassAmount, 0, 100)) / 100.0f;
    c.sharpen = waterTime();
    if (!grassLogged) LOG_INFO("grass: on %s, blades up to %.0f pixels", mapName.c_str(), c.twin[0]);
    grassLogged = true;
    const UINT list1 = steps ? stepsDataSrv : sceneColorSrv;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, true, list1)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, true, list1);
}

// 0.80: the steps the characters left - prints in the snow and the sand, dust on dry ground - onto the ground.
bool Renderer::Impl::applySteps() {
    stepsDone = true;
    const int kind = stepKind();
    const std::deque<footsteps::Step>& steps = footTracker.steps();
    if (steps.empty() || !ensureScene("footprints")) return true;
    ID3D12PipelineState* pso = alphaPipeline(stepsPs, stepsPipelines, upscale ? 1 : samples, "footprints");
    if (!pso) return true;
    if (!stepsData) {
        stepsData = createTexture2D(device, int(footsteps::Tracker::kMax * 2), 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                    D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        stepsDataSrv = stepsData ? allocSrv() : kNoSrv;
        if (!stepsData || stepsDataSrv == kNoSrv) {
            LOG_ERROR("footprints: no memory for their list: none");
            releaseCom(stepsData);
            releaseCom(stepsPs);
            return true;
        }
        stepsDataState = D3D12_RESOURCE_STATE_COPY_DEST;
        device->CreateShaderResourceView(stepsData, nullptr, cpuSrvHandle(stepsDataSrv));
    }
    const float now = waterTime(), life = stepLife();
    float data[footsteps::Tracker::kMax * 2][4] = {};
    size_t n = 0;
    for (const footsteps::Step& s : steps) {
        if (n >= footsteps::Tracker::kMax) break;
        data[2 * n][0] = s.x;
        data[2 * n][1] = s.y;
        data[2 * n][2] = s.z;
        data[2 * n][3] = s.angle;
        data[2 * n + 1][0] = std::clamp((now - s.born) / life, 0.0f, 1.0f);
        ++n;
    }
    const Upload u = allocUpload(sizeof data, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!u.cpu) return true;
    std::memcpy(u.cpu, data, sizeof data);
    D3D12_TEXTURE_COPY_LOCATION src{}, to{};
    src.pResource = slots[slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    src.PlacedFootprint.Footprint.Width = UINT(footsteps::Tracker::kMax * 2);
    src.PlacedFootprint.Footprint.Height = 1;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = UINT(sizeof data);  // 2048: a multiple of 256
    to.pResource = stepsData;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    if (!copySceneDepth()) return false;
    transition(stepsData, stepsDataState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
    transition(stepsData, stepsDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
    c.footprint = float(n);
    c.twin[0] = float(kind);
    if (!stepsLogged)
        LOG_INFO("footprints: %s on %s", kind == 1 ? "in the snow" : kind == 2 ? "in the sand" : "dust",
                 mapName.c_str());
    stepsLogged = true;
    const UINT list1 = stepsDataSrv;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, true, list1)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, true, list1);
}

void Renderer::Impl::releaseBloom() {
    Slot& s = slots[slot];  // what the open list recorded may still read them
    for (ID3D12Resource*& r : bloomTarget) {
        if (r) s.garbage.push_back(r);
        r = nullptr;
    }
    bloomWidth = bloomHeight = 0;
}

// 0.78: the bright parts at a quarter of the size, blurred twice each way, added; the filmic curve over all.
bool Renderer::Impl::applyBloom() {
    bloomDone = true;
    EffectTime timing(*this, kTimeBloom);
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (!ensureScene("bloom")) return true;
    const int w = sceneWidth, h = sceneHeight;
    if (!bloomDownPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, bloomDownPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&bloomDownPipeline))))
            bloomDownPipeline = nullptr;
        p = fullScreenPipeline(root, presentVs, dofBlurPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&bloomBlurPipeline))))
            bloomBlurPipeline = nullptr;
        if (!bloomDownPipeline || !bloomBlurPipeline) {
            LOG_ERROR("bloom: its pipelines failed: it stays off");
            releaseCom(bloomDownPs);
            return true;
        }
    }
    if (!bloomApplyPipelines[code]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, bloomApplyPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&bloomApplyPipelines[code])))) {
            bloomApplyPipelines[code] = nullptr;
            LOG_ERROR("bloom: the pipeline onto %u samples failed", 1u << code);
            return true;
        }
    }
    const int qw = std::max(1, (w + 3) / 4), qh = std::max(1, (h + 3) / 4);
    if (!bloomTarget[0] || qw != bloomWidth || qh != bloomHeight) {
        if (bloomFailed && qw == bloomWidth && qh == bloomHeight) return true;
        releaseBloom();
        bloomWidth = qw;
        bloomHeight = qh;
        bloomFailed = true;
        for (int i = 0; i < 2; ++i) {
            bloomTarget[i] = createTexture2D(device, qw, qh, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                             D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                             D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr);
            bloomState[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
            if (!bloomTarget[i]) {
                LOG_ERROR("bloom: no video memory at %dx%d: it stays off at this size", qw, qh);
                releaseBloom();
                bloomWidth = qw;
                bloomHeight = qh;
                return true;
            }
            device->CreateRenderTargetView(bloomTarget[i], nullptr, rtvHandle(kRtvBloom + UINT(i)));
            device->CreateShaderResourceView(bloomTarget[i], nullptr, cpuSrvHandle(bloomSrv[i]));
        }
        bloomFailed = false;
    }
    if (!copySceneColor()) return false;
    Constants c{};
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[2] = float(qw);
    c.uiPixels[3] = float(qh);
    // The bright parts: over this. 0.83.1: was 0.72 and 1.4 times - a light paved square (Prontera's) bloomed whole
    // and went white; now only what is near white glows, and less.
    c.footprint = 0.88f;
    const bool bloom = effects.bloom && effects.bloomStrength > 0;
    c.sharpen = bloom ? 0.6f * float(std::clamp(effects.bloomStrength, 0, 200)) / 100.0f : 0.0f;
    c.twin[1] = effects.filmic ? 1.0f : 0.0f;
    c.twin[2] = effects.filmGrain ? float(std::clamp(effects.filmGrainAmount, 0, 100)) / 100.0f : 0.0f;  // 0.83
    c.twin[3] = mapFadeNow();
    c.sourceRect[2] = std::fmod(waterTime() * 60.0f, 4096.0f);  // the grain's seed, new each frame
    if (bloom) {
        if (!effectPass(bloomDownPipeline, bloomTarget[0], bloomState[0], rtvHandle(kRtvBloom), qw, qh, c,
                        sceneColorSrv, true))
            return false;
        for (int k = 0; k < 4; ++k) {  // across, down, then again twice as wide
            const int i = k % 2;
            transition(bloomTarget[i], bloomState[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            c.sourceRect[0] = i == 0 ? float(1 + k / 2) : 0.0f;
            c.sourceRect[1] = i == 0 ? 0.0f : float(1 + k / 2);
            const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(kRtvBloom + UINT(1 - i));
            if (!effectPass(bloomBlurPipeline, bloomTarget[1 - i], bloomState[1 - i], rtv, qw, qh, c, bloomSrv[i],
                            true))
                return false;
        }
    }
    transition(bloomTarget[0], bloomState[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return upscale ? effectPass(bloomApplyPipelines[code], world, worldState, rtvHandle(kRtvWorld), w, h, c,
                                bloomSrv[0], true)
                   : effectPass(bloomApplyPipelines[code], target, targetState, rtvHandle(kRtvTarget), w, h, c,
                                bloomSrv[0], true);
}

void Renderer::Impl::releaseDof() {
    Slot& s = slots[slot];  // what the open list recorded may still read them
    for (ID3D12Resource*& r : dofTarget) {
        if (r) s.garbage.push_back(r);
        r = nullptr;
    }
    for (ID3D12Resource*& r : dofFocus) {  // made again cleared: the next focus is taken at once
        if (r) s.garbage.push_back(r);
        r = nullptr;
    }
    dofWidth = dofHeight = 0;
}

bool Renderer::Impl::applyDof() {
    dofDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeDof);  // 0.39.2
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (!dofDownPs || !dofBlurPs || !dofApplyPs || !dofFocusPs || !ensureScene("depth of field")) return true;
    const int w = sceneWidth, h = sceneHeight;
    if (!dofDownPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, dofDownPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&dofDownPipeline))))
            dofDownPipeline = nullptr;
        p = fullScreenPipeline(root, presentVs, dofBlurPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&dofBlurPipeline))))
            dofBlurPipeline = nullptr;
        p = fullScreenPipeline(root, presentVs, dofFocusPs, DXGI_FORMAT_R32_FLOAT, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&dofFocusPipeline))))
            dofFocusPipeline = nullptr;
        if (!dofDownPipeline || !dofBlurPipeline || !dofFocusPipeline) {
            LOG_ERROR("depth of field: its pipelines failed: it stays off");
            return true;
        }
    }
    if (!dofApplyPipelines[code]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, dofApplyPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&dofApplyPipelines[code])))) {
            dofApplyPipelines[code] = nullptr;
            LOG_ERROR("depth of field: the pipeline onto %u samples failed", 1u << code);
            return true;
        }
    }
    const int qw = std::max(1, (w + 3) / 4), qh = std::max(1, (h + 3) / 4);
    if (!dofTarget[0] || qw != dofWidth || qh != dofHeight) {
        if (dofFailed && qw == dofWidth && qh == dofHeight) return true;
        releaseDof();
        dofWidth = qw;
        dofHeight = qh;
        dofFailed = true;
        for (int i = 0; i < 2; ++i) {
            dofTarget[i] = createTexture2D(device, qw, qh, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                           D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           nullptr);
            dofState[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
            if (!dofTarget[i]) {
                LOG_ERROR("depth of field: no video memory at %dx%d: it stays off at this size", qw, qh);
                releaseDof();
                dofWidth = qw;  // remembered as failed
                dofHeight = qh;
                return true;
            }
            device->CreateRenderTargetView(dofTarget[i], nullptr, rtvHandle(kRtvDof + UINT(i)));
            device->CreateShaderResourceView(dofTarget[i], nullptr, cpuSrvHandle(dofSrv[i]));
        }
        dofFailed = false;
        LOG_INFO("depth of field: blurred at %dx%d from the %dx%d world", qw, qh, w, h);
    }
    if (!dofFocus[0]) {
        D3D12_CLEAR_VALUE none{};
        none.Format = DXGI_FORMAT_R32_FLOAT;
        for (int i = 0; i < 2; ++i) {
            dofFocus[i] = createTexture2D(device, 1, 1, 1, DXGI_FORMAT_R32_FLOAT,
                                          D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                          &none);
            dofFocusState[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
            if (!dofFocus[i]) return true;  // tried again the next frame
            device->CreateRenderTargetView(dofFocus[i], nullptr, rtvHandle(kRtvDofFocus + UINT(i)));
            device->CreateShaderResourceView(dofFocus[i], nullptr, cpuSrvHandle(dofFocusSrv[i]));
            const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // no focus yet: the first is taken as it is
            list->ClearRenderTargetView(rtvHandle(kRtvDofFocus + UINT(i)), zero, 0, nullptr);
        }
    }
    if (!copySceneColor() || !copySceneDepth()) return false;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(upscale ? worldScale : scale);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[2] = float(qw);
    c.uiPixels[3] = float(qh);
    // Where the blur starts: 1-15 % of the focus's distance (about 3-45 units at the usual camera distance).
    c.footprint = 0.01f + 0.14f * float(std::clamp(effects.dofRange, 0, 100)) / 100.0f;
    c.sharpen = float(std::clamp(effects.dofStrength, 0, 100)) / 100.0f;
    // The focus: from the last frame's, into the other texel - a share of the way that follows the time since then
    // (0.22.5: 0.06 s to most of it - the camera zooms without the focus lagging; the median already holds still
    // against a character's frames), the first one or after a pause all of it.
    LARGE_INTEGER now, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    const double dt =
        dofFocusTime.QuadPart ? double(now.QuadPart - dofFocusTime.QuadPart) / double(freq.QuadPart) : 1.0;
    dofFocusTime = now;
    c.twin[3] = dt > 0.5 ? 1.0f : float(1.0 - std::exp(-dt / 0.06));
    // 1.4.2: the focus where the camera looks - the character - read from the game: the depths around the middle
    // of the screen took a tree's crown or a roof the character stood under, and the ground around went soft.
    c.reproject[0] = cameraValid ? cameraTargetZ : 0.0f;
    const int was = dofFocusNow;
    dofFocusNow = 1 - dofFocusNow;
    transition(dofFocus[was], dofFocusState[was], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (!effectPass(dofFocusPipeline, dofFocus[dofFocusNow], dofFocusState[dofFocusNow],
                    rtvHandle(kRtvDofFocus + UINT(dofFocusNow)), 1, 1, c, dofFocusSrv[was], true))
        return false;
    transition(dofFocus[dofFocusNow], dofFocusState[dofFocusNow], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (!effectPass(dofDownPipeline, dofTarget[0], dofState[0], rtvHandle(kRtvDof), qw, qh, c, sceneColorSrv, true))
        return false;
    for (int i = 0; i < 2; ++i) {  // across, then down
        transition(dofTarget[i], dofState[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        c.sourceRect[0] = i == 0 ? 1.0f : 0.0f;
        c.sourceRect[1] = i == 0 ? 0.0f : 1.0f;
        if (!effectPass(dofBlurPipeline, dofTarget[1 - i], dofState[1 - i], rtvHandle(kRtvDof + UINT(1 - i)), qw, qh,
                        c, dofSrv[i], true))
            return false;
    }
    transition(dofTarget[0], dofState[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const UINT focus = dofFocusSrv[dofFocusNow];
    return upscale ? effectPass(dofApplyPipelines[code], world, worldState, rtvHandle(kRtvWorld), w, h, c, dofSrv[0],
                                true, focus)
                   : effectPass(dofApplyPipelines[code], target, targetState, rtvHandle(kRtvTarget), w, h, c,
                                dofSrv[0], true, focus);
}

bool Renderer::Impl::finishWorld() {
    if (aoDue() && !applyAo()) return false;
    if (sunShadowDue() && !applySunShadows()) return false;  // 0.77
    if (grassDue() && !applyGrass()) return false;            // 0.81
    if (stepsDue() && !applySteps()) return false;            // 0.80: on the ground, under the night
    if (nightDue() && !applyNight()) return false;    // 0.63: the night first - the lights shine out of it
    if (torchDue() && !applyTorches()) return false;  // 0.23: lit and burning before the colours
    if (embersDue() && !applyEmbers()) return false;  // 0.82: the skills' sparks
    if (waterfallsDue() && !applyWaterfalls()) return false;  // 0.65: the maps' waterfalls the game never shows
    if (floorReflectDue() && !applyFloorReflections()) return false;  // 0.72: lit, before the haze
    if (hazeDue() && !applyHaze()) return false;      // 0.34: far off and over the ground, before the colours
    if (skyDue() && !applySky()) return false;        // 0.69: the sky where the haze filled the void
    if (shaftsDue() && !applyShafts()) return false;  // 0.73: the light in the air
    if (flareDue() && !applyFlare()) return false;    // 0.98: the sun's flare
    if (heatDue() && !applyHeat()) return false;      // 0.35: the hot air shimmering over all of that
    if (bloomDue() && !applyBloom()) return false;  // 0.78: before the colours
    if (gradeDue() && !applyGrade()) return false;
    if (dofDue() && !applyDof()) return false;
    return !weatherDue() || applyWeather();  // 0.39: the weather in front of all of it, sharp
}

}  // namespace uagfx::gpu
