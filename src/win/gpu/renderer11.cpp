#include "win/gpu/renderer11.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "core/ai/kinds.hpp"
#include "core/d3d7.hpp"
#include "core/d3d_map.hpp"
#include "core/daylight.hpp"
#include "core/living.hpp"
#include "core/outdoors.hpp"
#include "core/effect_exe.hpp"
#include "core/effect_light.hpp"
#include "core/foliage.hpp"
#include "core/full_colour.hpp"
#include "core/light_mode.hpp"
#include "core/game_camera.hpp"
#include "core/pipeline_key.hpp"
#include "core/png_write.hpp"
#include "core/region_look.hpp"
#include "core/scale.hpp"
#include "core/shader_cache.hpp"
#include "core/twin_grid.hpp"
#include "core/water.hpp"
#include "core/window_plan.hpp"
#include "win/gpu/adapters.hpp"
#include "win/gpu/ai11.hpp"
#include "win/gpu/draw_constants.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::gpu {
namespace {

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

// Direct3D 11.0 clears whole views: a part of the target is cleared by a triangle of the colour at the depth.
const char* const kClearSource = R"HLSL(
cbuffer ClearParams : register(b0) {
    float4 clearColor;
    float clearZ;
};
float4 clear_vs(uint id : SV_VertexID) : SV_Position {
    float2 t = float2((id << 1) & 2, id & 2);
    return float4(t * float2(2, -2) + float2(-1, 1), clearZ, 1);
}
float4 clear_ps() : SV_Target { return clearColor; }
)HLSL";

struct ClearConstants {
    float color[4];
    float z;
    float pad[3];
};

constexpr UINT kConstantBytes = (sizeof(Constants) + 15) / 16 * 16;

D3D11_BOX boxOf(const Rect& r) { return D3D11_BOX{UINT(r.left), UINT(r.top), 0, UINT(r.right), UINT(r.bottom), 1}; }

std::string levelName(unsigned level) {  // D3D_FEATURE_LEVEL 0xb000 -> "11_0"
    return std::to_string((level >> 12) & 15) + "_" + std::to_string((level >> 8) & 15);
}

// 0.89: a B8G8R8A8 texture's bytes with its levels (the pool's measure)
size_t textureBytes(int width, int height, int levels) {
    size_t bytes = 0;
    for (int i = 0; i < levels; ++i) bytes += size_t(std::max(1, width >> i)) * size_t(std::max(1, height >> i)) * 4;
    return bytes;
}

}  // namespace

Renderer11::~Renderer11() {
    if (timing_ && ctx_ && !lost_) {  // 0.46.1: the frames still on their way, then the GPU's time since the last line
        for (UINT i = 0; i < kTimedFrames; ++i) readTimes((timeSlot_ + i) % kTimedFrames, true);
        logGpu(GetTickCount64());
    }
    delete ai_;  // its thread first: it makes textures on the device
    ai_ = nullptr;
    stopVariants();  // 2.5.16: its thread too
    if (ctx_) ctx_->ClearState();
    releaseSwapChain();
    for (auto& t : textures_) {
        dropMaterial(t.second);  // 1.14
        drop(t.second.flat);     // 2.5.20
        drop(t.second.srv);
        drop(t.second.texture);
    }
    for (Pooled& p : pool_) {  // 0.89
        drop(p.srv);
        drop(p.texture);
    }
    pool_.clear();
    drop(white_.srv);
    drop(white_.texture);
    dropWorld();  // 2.5.16
    dropTarget(target_);
    dropTarget(game_);
    dropTarget(scene_);
    drop(dsv_);
    drop(depth_);
    drop(staging_);
    for (auto& b : blends_) drop(b.second);
    for (auto& d : depths_) drop(d.second);
    for (auto& r : rasters_) drop(r.second);
    for (auto& s : samplers_) drop(s.second);
    for (FrameTimes& f : times_) {
        drop(f.disjoint);
        for (ID3D11Query*& q : f.stamps) drop(q);
        for (auto& pair : f.effect)
            for (ID3D11Query*& q : pair) drop(q);
    }
    if (batch_.open())  // 0.90: the views a gathered call held
        for (ID3D11ShaderResourceView* v : issue_.srvs)
            if (v) v->Release();
    drop(vertices_);
    drop(indices_);
    drop(quadIndices_);
    drop(constants_);
    drop(clearConstants_);
    for (ID3D11InputLayout*& l : layout_) drop(l);
    drop(drawVs_);
    drop(drawPs_);
    drop(waterVs_);  // 0.99
    drop(waterPs_);
    drop(fireflyPs_);  // 1.0.1
    drop(materialPs_);  // 1.14
    drop(drawGlowPs_);  // 1.0
    drop(drawLeanPs_);  // 2.5
    drop(drawGlowLeanPs_);
    drop(mistPs_);
    drop(skyPs_);
    drop(weatherPs_);
    drop(aoPs_);  // 2.5.1
    drop(aoBlurPs_);
    drop(aoApplyPs_);
    dropTarget(aoTarget_[0]);
    dropTarget(aoTarget_[1]);
    drop(torchFirePs_);  // 2.5.2
    drop(dustPs_);
    drop(bloomDownPs_);
    drop(dofBlurPs_);
    drop(bloomApplyPs_);
    dropTarget(bloom_[0]);
    dropTarget(bloom_[1]);
    drop(stepsPs_);
    drop(stepsDataSrv_);
    drop(stepsData_);
    drop(trampleSrv_);  // 2.6.8
    drop(trampleData_);
    drop(smokePs_);
    drop(smokeGlowPs_);
    drop(lavaPs_);
    drop(lavaGlowPs_);
    drop(heatPs_);  // 2.5.3
    drop(heatDataSrv_);
    drop(heatData_);
    drop(rayPs_);
    drop(rayGlowPs_);
    drop(embersPs_);
    drop(embersDataSrv_);
    drop(embersData_);
    drop(floorReflectPs_);
    drop(halfOverPs_);
    dropTarget(floor_);
    drop(splashPs_);
    drop(groundLifePs_);  // 2.5.4
    drop(grassPs_);
    drop(groundCellsSrv_);
    drop(groundCellsData_);
    drop(crittersVs_);  // 2.5.4
    drop(crittersPs_);
    drop(crittersDataSrv_);
    drop(crittersData_);
    drop(shaftsPs_);
    drop(flarePs_);
    dropTarget(shafts_);  // 2.5.7
    drop(hillsSrv_);  // 2.5.9
    drop(hillsData_);
    drop(seamVertices_);
    drop(seamIndices_);
    drop(mirrorVs_);
    drop(mirrorPs_);
    drop(mirrorLayout_);
    drop(mirrorVb_);
    dropTarget(mirror_);
    drop(mirrorDsv_);
    drop(mirrorDepth_);
    drop(waterGlowPs_);
    drop(glowDownPs_);
    drop(glowUpPs_);
    drop(glowAddPs_);
    dropTarget(glow_);
    for (Target& t : glowChain_) dropTarget(t);
    drop(depthCopySrv_);
    drop(depthCopyDsv_);
    drop(depthCopy_);
    drop(presentVs_);
    drop(presentPs_);
    drop(gradePs_);
    drop(torchLightPs_);
    drop(nightPs_);
    drop(lightDataSrv_);
    drop(lightData_);
    drop(shadowVs_);
    drop(shadowPs_);
    drop(shadowBlurPs_);
    drop(shadowApplyPs_);
    drop(shadowLayout_);
    drop(shadowVb_);
    drop(depthSrv_);
    drop(readOnlyDsv_);
    drop(maxBlend_);
    drop(mulBlend_);
    for (Target& m : shadowMask_) dropTarget(m);
    drop(clearVs_);
    drop(clearPs_);
    drop(factory_);
    drop(ctx_);
    drop(device_);
    // dll_ stays loaded: the game closes DirectDraw when it changes modes and opens it again.
}

void Renderer11::failed(const char* what, long hr) {
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
        (device_ && FAILED(device_->GetDeviceRemovedReason()))) {
        if (!lost_)
            LOG_ERROR("Direct3D 11: %s - the graphics card was lost (0x%08lx, reason 0x%08lx); the picture stops here, "
                      "restart the game",
                      what, static_cast<unsigned long>(hr),
                      static_cast<unsigned long>(device_ ? device_->GetDeviceRemovedReason() : 0));
        lost_ = true;
        return;
    }
    if (failures_[{what, hr}]++ == 0)
        LOG_ERROR("Direct3D 11: %s failed: 0x%08lx", what, static_cast<unsigned long>(hr));
}

bool Renderer11::create(const std::string& shaderCache, Renderer::StartSink sink, void* sinkContext,
                        std::string& error) {
    dll_ = LoadLibraryA("d3d11.dll");
    const auto createDevice = dll_ ? reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
                                         reinterpret_cast<void*>(GetProcAddress(dll_, "D3D11CreateDevice")))
                                   : nullptr;
    if (!createDevice) {
        error = "d3d11.dll is missing";
        return false;
    }
    // 11_1 first where the runtime knows it (E_INVALIDARG where it does not: Windows 7 without its platform update).
    const D3D_FEATURE_LEVEL levels[4] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                         D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_10_0;
    const auto make = [&](IDXGIAdapter* a) {
        const D3D_DRIVER_TYPE type = a ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
        HRESULT r = createDevice(a, type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 4, D3D11_SDK_VERSION,
                                 &device_, &got, &ctx_);
        if (r == E_INVALIDARG)
            r = createDevice(a, type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels + 1, 3, D3D11_SDK_VERSION,
                             &device_, &got, &ctx_);
        return r;
    };
    // 0.50.1: the fast card of several (a laptop's NVIDIA or AMD, not its integrated Intel), else Windows' first.
    HRESULT hr = E_FAIL;
    std::vector<AdapterFacts> all;
    const auto createFactory = reinterpret_cast<HRESULT(WINAPI*)(REFIID, void**)>(
        reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("dxgi.dll"), "CreateDXGIFactory1")));
    IDXGIFactory1* listing = nullptr;
    if (createFactory && SUCCEEDED(createFactory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&listing)))) {
        std::vector<IDXGIAdapter1*> adapters = orderedAdapters(listing, &all);
        for (IDXGIAdapter1* a : adapters) {
            hr = make(a);
            if (SUCCEEDED(hr) && device_ && ctx_) break;
            drop(ctx_);
            drop(device_);
        }
        releaseAdapters(adapters);
        listing->Release();
    }
    if (FAILED(hr) || !device_ || !ctx_) hr = make(nullptr);
    if (FAILED(hr) || !device_ || !ctx_) {
        char text[96];
        std::snprintf(text, sizeof text, "no Direct3D 11 device of feature level 10_0 or better (0x%08lx)",
                      static_cast<unsigned long>(hr));
        error = text;
        return false;
    }
    level_ = unsigned(got);
    IDXGIDevice* dxgi = nullptr;
    IDXGIAdapter* adapter = nullptr;
    if (SUCCEEDED(device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi))) &&
        SUCCEEDED(dxgi->GetAdapter(&adapter))) {
        DXGI_ADAPTER_DESC d{};
        if (SUCCEEDED(adapter->GetDesc(&d))) {
            char name[256];
            const int n = WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, sizeof name, nullptr, nullptr);
            adapter_ = n > 0 ? std::string(name) : std::string();
        }
        adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory_));
    }
    drop(adapter);
    drop(dxgi);
    if (all.size() > 1) LOG_INFO("graphics cards: %s - Direct3D 11 on %s", adaptersText(all).c_str(), adapter_.c_str());
    if (!factory_) {
        error = "no DXGI factory of the Direct3D 11 device";
        return false;
    }
    UINT support = 0;
    bgraVertices_ = SUCCEEDED(device_->CheckFormatSupport(DXGI_FORMAT_B8G8R8A8_UNORM, &support)) &&
                    (support & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER);
    // 2.5.20: super-light's 16-bit world - B5G6R5 is optional before Windows 8 (its 11.1 runtime)
    const UINT world16 = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET |
                         D3D11_FORMAT_SUPPORT_SHADER_SAMPLE | D3D11_FORMAT_SUPPORT_BLENDABLE;
    UINT depth16 = 0;
    world16Ok_ = SUCCEEDED(device_->CheckFormatSupport(DXGI_FORMAT_B5G6R5_UNORM, &support)) &&
                 (support & world16) == world16 &&
                 SUCCEEDED(device_->CheckFormatSupport(DXGI_FORMAT_D16_UNORM, &depth16)) &&
                 (depth16 & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL);
    if (!compileShaders(shaderCache, sink, sinkContext, error)) return false;
    // The game's draws' and copies' constants, a part's clear's.
    D3D11_BUFFER_DESC b{};
    b.ByteWidth = kConstantBytes;
    b.Usage = D3D11_USAGE_DYNAMIC;
    b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    b.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&b, nullptr, &constants_))) {
        error = "no constant buffer";
        return false;
    }
    b.ByteWidth = sizeof(ClearConstants);
    if (FAILED(device_->CreateBuffer(&b, nullptr, &clearConstants_))) {
        error = "no constant buffer";
        return false;
    }
    // 0.90: the quads' index pattern (core/draw_batch.hpp); without it each quad is drawn by itself as before
    const std::vector<uint16_t> quads = batch::quadIndices(batch::kMaxQuads);
    D3D11_BUFFER_DESC q{};
    q.ByteWidth = UINT(quads.size() * sizeof(uint16_t));
    q.Usage = D3D11_USAGE_IMMUTABLE;
    q.BindFlags = D3D11_BIND_INDEX_BUFFER;
    const D3D11_SUBRESOURCE_DATA quadData{quads.data(), 0, 0};
    if (FAILED(device_->CreateBuffer(&q, &quadData, &quadIndices_))) {
        quadIndices_ = nullptr;
        LOG_INFO("Direct3D 11: no index pattern for the quads - each of the game's draws goes by itself");
    }
    // A texture the game has not given (or not yet): white, as Direct3D 7 reads a missing one (states.md §5).
    D3D11_TEXTURE2D_DESC t{};
    t.Width = t.Height = 1;
    t.MipLevels = t.ArraySize = 1;
    t.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    t.SampleDesc.Count = 1;
    t.Usage = D3D11_USAGE_IMMUTABLE;
    t.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const uint32_t whitePixel = 0xFFFFFFFF;
    const D3D11_SUBRESOURCE_DATA init{&whitePixel, 4, 4};
    if (FAILED(device_->CreateTexture2D(&t, &init, &white_.texture)) ||
        FAILED(device_->CreateShaderResourceView(white_.texture, nullptr, &white_.srv))) {
        error = "no texture of one white pixel";
        return false;
    }
    white_.width = white_.height = 1;
    // 0.46.1: the GPU's time a frame - a disjoint query and its timestamps for each of the frames in flight.
    timing_ = true;
    for (FrameTimes& f : times_) {
        D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        timing_ = timing_ && SUCCEEDED(device_->CreateQuery(&q, &f.disjoint));
        q.Query = D3D11_QUERY_TIMESTAMP;
        for (ID3D11Query*& t : f.stamps) timing_ = timing_ && SUCCEEDED(device_->CreateQuery(&q, &t));
        for (auto& pair : f.effect)  // 0.89
            for (ID3D11Query*& t : pair) timing_ = timing_ && SUCCEEDED(device_->CreateQuery(&q, &t));
    }
    LOG_INFO("Direct3D 11: %s, feature level %s%s", adapter_.c_str(), levelName(level_).c_str(),
             bgraVertices_ ? "" : ", vertex colours turned around for it");
    return true;
}

bool Renderer11::compileShaders(const std::string& cacheFile, Renderer::StartSink sink, void* sinkContext,
                                std::string& error) {
    // Shader model 5 at feature level 11_0 and up - the shaders Direct3D 12 compiles too; 4 below it.
    const bool sm5 = level_ >= unsigned(D3D_FEATURE_LEVEL_11_0);
    const char* vs = sm5 ? "vs_5_0" : level_ >= unsigned(D3D_FEATURE_LEVEL_10_1) ? "vs_4_1" : "vs_4_0";
    const char* ps = sm5 ? "ps_5_0" : level_ >= unsigned(D3D_FEATURE_LEVEL_10_1) ? "ps_4_1" : "ps_4_0";
    struct Want {
        const char* entry;
        const char* profile;
        const char* source;
        std::vector<uint8_t> code;
        bool optional = false;  // 0.99: an effect's - without it the effect is not drawn, the game is
        bool safe = false;      // 2.2.2: made without the newest features (UAGFX_SAFE)
        bool lean = false;      // 2.5: with UAGFX_LEAN (draw_lean_ps: the world's extras left out)
    } wants[55] = {{"draw_vs", vs, shaderSource(), {}},    {"draw_ps", ps, shaderSource(), {}},
                   {"present_vs", vs, shaderSource(), {}}, {"present_ps", ps, shaderSource(), {}},
                   {"clear_vs", vs, kClearSource, {}},     {"clear_ps", ps, kClearSource, {}},
                   {"grade_ps", ps, shaderSource(), {}},  // 0.59: the colours (Effects page)
                   {"shadow_vs", vs, shaderSource(), {}},  {"shadow_ps", ps, shaderSource(), {}},  // 0.60.1
                   {"shadow_blur_ps", ps, shaderSource(), {}}, {"shadow_apply_ps", ps, shaderSource(), {}},
                   {"torch_light_ps", ps, shaderSource(), {}},  // 0.61
                   {"night_ps", ps, shaderSource(), {}},        // 0.63
                   {"water_vs", vs, shaderSource(), {}, true},  {"water_ps", ps, shaderSource(), {}, true},  // 0.99
                   {"draw_glow_ps", ps, shaderSource(), {}, true}, {"water_glow_ps", ps, shaderSource(), {}, true},
                   {"glow_down_ps", ps, shaderSource(), {}, true}, {"glow_up_ps", ps, shaderSource(), {}, true},
                   {"glow_add_ps", ps, shaderSource(), {}, true},  // 1.0
                   {"firefly_ps", ps, shaderSource(), {}, true},  // 1.0.1
                   {"material_ps", ps, shaderSource(), {}, true},  // 1.14
                   {"draw_lean_ps", ps, shaderSource(), {}, true, false, true},  // 2.5
                   {"draw_glow_lean_ps", ps, shaderSource(), {}, true, false, true},
                   {"mist_ps", ps, shaderSource(), {}, true},  // 2.5: the mists, the sky, the weather
                   {"sky_ps", ps, shaderSource(), {}, true},
                   {"weather_ps", ps, shaderSource(), {}, true},
                   {"ao_ps", ps, shaderSource(), {}, true},  // 2.5.1: the soft shadows
                   {"ao_blur_ps", ps, shaderSource(), {}, true},
                   {"ao_apply_ps", ps, shaderSource(), {}, true},
                   {"torch_fire_ps", ps, shaderSource(), {}, true},  // 2.5.2: the new flame, the maps' dust
                   {"dust_ps", ps, shaderSource(), {}, true},
                   {"bloom_down_ps", ps, shaderSource(), {}, true},  // 2.5.2: the bloom, the map's fade
                   {"dof_blur_ps", ps, shaderSource(), {}, true},
                   {"bloom_apply_ps", ps, shaderSource(), {}, true},
                   {"steps_ps", ps, shaderSource(), {}, true},  // 2.5.2: the footprints, the smoke, the lava
                   {"smoke_ps", ps, shaderSource(), {}, true},
                   {"smoke_glow_ps", ps, shaderSource(), {}, true},
                   {"lava_ps", ps, shaderSource(), {}, true},
                   {"lava_glow_ps", ps, shaderSource(), {}, true},
                   {"heat_ps", ps, shaderSource(), {}, true},  // 2.5.3: the heat haze, the forest's beams
                   {"ray_ps", ps, shaderSource(), {}, true},
                   {"ray_glow_ps", ps, shaderSource(), {}, true},
                   {"embers_ps", ps, shaderSource(), {}, true},  // 2.5.3: sparks, floors' reflections, splashes
                   {"floor_reflect_ps", ps, shaderSource(), {}, true},
                   {"half_over_ps", ps, shaderSource(), {}, true},
                   {"splash_ps", ps, shaderSource(), {}, true},
                   {"ground_life_ps", ps, shaderSource(), {}, true},  // 2.5.4: the living ground, the grass
                   {"grass_ps", ps, shaderSource(), {}, true},
                   {"critters_vs", vs, shaderSource(), {}, true},  // 2.5.4: the living world
                   {"critters_ps", ps, shaderSource(), {}, true},
                   {"shafts_ps", ps, shaderSource(), {}, true},  // 2.5.5: shafts, flare, DoF
                   {"flare_ps", ps, shaderSource(), {}, true},
                   {"mirror_vs", vs, shaderSource(), {}, true},  // 2.5.9: the world's reflections in the water
                   {"mirror_ps", ps, shaderSource(), {}, true}};
    const uint64_t key = shader_cache::keyOf(std::string(shaderSource()) + kClearSource + "direct3d 11");
    std::vector<shader_cache::Shader> kept;
    {
        std::ifstream f(cacheFile, std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (!bytes.empty()) kept = shader_cache::unpack(key, bytes);
    }
    int missing = 0;
    for (Want& w : wants) {
        for (const shader_cache::Shader& s : kept)
            if (shader_cache::matches(s, w.entry, w.profile, w.safe)) w.code = s.code;
        if (!w.code.empty() && w.safe) noteSafeShader();  // 2.2.2: kept from a start that made it so
        missing += w.code.empty() ? 1 : 0;
    }
    if (sink) sink(sinkContext, start::Progress{missing ? start::Stage::ShadersBuild : start::Stage::ShadersLoad, 0,
                                                missing});
    if (missing) {
        HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
        if (!compiler) compiler = LoadLibraryA("d3dcompiler_43.dll");  // the DirectX runtime of 2010 (Windows 7)
        const auto compile = compiler ? reinterpret_cast<pD3DCompile>(
                                            reinterpret_cast<void*>(GetProcAddress(compiler, "D3DCompile")))
                                      : nullptr;
        if (!compile) {
            error = "d3dcompiler_47.dll is missing (Windows 7: the update KB4019990 or the DirectX runtime has it)";
            return false;
        }
        int done = 0;
        for (Want& w : wants) {
            if (!w.code.empty()) continue;
            // As Direct3D 12 does (0.39.2): the optimiser fails now and then - then again with less of it.
            // 0.76: and at last without the newest features (UAGFX_SAFE), as pipeline.cpp does.
            static const UINT kLevels[4] = {D3DCOMPILE_OPTIMIZATION_LEVEL3, D3DCOMPILE_OPTIMIZATION_LEVEL1,
                                            D3DCOMPILE_SKIP_OPTIMIZATION, D3DCOMPILE_OPTIMIZATION_LEVEL1};
            static const D3D_SHADER_MACRO kSafe[2] = {{"UAGFX_SAFE", "1"}, {nullptr, nullptr}};
            static const D3D_SHADER_MACRO kLean[2] = {{"UAGFX_LEAN", "1"}, {nullptr, nullptr}};
            static const D3D_SHADER_MACRO kLeanSafe[3] = {
                {"UAGFX_LEAN", "1"}, {"UAGFX_SAFE", "1"}, {nullptr, nullptr}};
            HRESULT hr = E_FAIL;
            std::string said;
            for (int i = 0; i < 4 && FAILED(hr); ++i) {
                ID3DBlob* code = nullptr;
                ID3DBlob* errors = nullptr;
                const D3D_SHADER_MACRO* macros = i == 3 ? (w.lean ? kLeanSafe : kSafe) : w.lean ? kLean : nullptr;
                hr = compile(w.source, std::strlen(w.source), "uaro-graphics.hlsl", macros, nullptr, w.entry,
                             w.profile, kLevels[i], 0, &code, &errors);
                if (SUCCEEDED(hr) && i == 3) {
                    LOG_ERROR("Direct3D 11: shader %s compiled without the newest features", w.entry);
                    w.safe = true;
                    noteSafeShader();
                }
                if (FAILED(hr) && i == 0 && errors)
                    said = std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
                if (SUCCEEDED(hr) && code) {
                    const auto* p = static_cast<const uint8_t*>(code->GetBufferPointer());
                    w.code.assign(p, p + code->GetBufferSize());
                }
                drop(code);
                drop(errors);
            }
            if (w.code.empty() && w.optional) {  // 0.99
                LOG_ERROR("Direct3D 11: shader %s did not compile - its effect is not drawn", w.entry);
                continue;
            }
            if (w.code.empty()) {
                const size_t line = said.find("error");
                const std::string why =
                    said.empty() ? std::string("no code") : said.substr(line == std::string::npos ? 0 : line, 160);
                error = std::string("shader ") + w.entry + " (" + w.profile + "): " + why;
                return false;
            }
            if (sink) sink(sinkContext, start::Progress{start::Stage::ShadersBuild, ++done, missing});
        }
        std::vector<shader_cache::Shader> all;
        for (const Want& w : wants) all.push_back({w.entry, shader_cache::profileOf(w.profile, w.safe), w.code});
        shader_variants::Key variant;  // 2.5.16: the draw shaders of the game's states kept with them
        for (const shader_cache::Shader& s : kept)
            if (shader_variants::parseName(s.entry, variant)) all.push_back(s);
        const std::vector<uint8_t> bytes = shader_cache::pack(key, all);
        win::writeFileAtomic(cacheFile, bytes.data(), bytes.size());  // another client starting reads it whole or not
    }
    LOG_INFO("Direct3D 11: shaders (%s) %s", ps, missing ? "compiled and kept for the next start" : "kept from the "
                                                                                                  "last start");
    const auto vsOf = [&](const Want& w, ID3D11VertexShader*& out) {
        return SUCCEEDED(device_->CreateVertexShader(w.code.data(), w.code.size(), nullptr, &out));
    };
    const auto psOf = [&](const Want& w, ID3D11PixelShader*& out) {
        return SUCCEEDED(device_->CreatePixelShader(w.code.data(), w.code.size(), nullptr, &out));
    };
    if (!vsOf(wants[0], drawVs_) || !psOf(wants[1], drawPs_) || !vsOf(wants[2], presentVs_) ||
        !psOf(wants[3], presentPs_) || !vsOf(wants[4], clearVs_) || !psOf(wants[5], clearPs_) ||
        !psOf(wants[6], gradePs_) || !vsOf(wants[7], shadowVs_) || !psOf(wants[8], shadowPs_) ||
        !psOf(wants[9], shadowBlurPs_) || !psOf(wants[10], shadowApplyPs_) || !psOf(wants[11], torchLightPs_) ||
        !psOf(wants[12], nightPs_)) {
        error = "the shaders' code was refused";
        return false;
    }
    if (wants[13].code.empty() || wants[14].code.empty() || !vsOf(wants[13], waterVs_) || !psOf(wants[14], waterPs_)) {
        drop(waterVs_);  // 0.99: the water as the game draws it
        drop(waterPs_);
    }
    ID3D11PixelShader** glows[5] = {&drawGlowPs_, &waterGlowPs_, &glowDownPs_, &glowUpPs_, &glowAddPs_};  // 1.0
    for (int k = 0; k < 5; ++k)
        if (wants[15 + k].code.empty() || !psOf(wants[15 + k], *glows[k])) *glows[k] = nullptr;
    if (wants[20].code.empty() || !psOf(wants[20], fireflyPs_)) fireflyPs_ = nullptr;  // 1.0.1
    if (wants[21].code.empty() || !psOf(wants[21], materialPs_)) materialPs_ = nullptr;  // 1.14
    if (wants[22].code.empty() || !psOf(wants[22], drawLeanPs_)) drawLeanPs_ = nullptr;  // 2.5
    if (wants[23].code.empty() || !psOf(wants[23], drawGlowLeanPs_)) drawGlowLeanPs_ = nullptr;
    if (wants[24].code.empty() || !psOf(wants[24], mistPs_)) mistPs_ = nullptr;  // 2.5
    if (wants[25].code.empty() || !psOf(wants[25], skyPs_)) skyPs_ = nullptr;
    if (wants[26].code.empty() || !psOf(wants[26], weatherPs_)) weatherPs_ = nullptr;
    if (wants[27].code.empty() || !psOf(wants[27], aoPs_)) aoPs_ = nullptr;  // 2.5.1
    if (wants[28].code.empty() || !psOf(wants[28], aoBlurPs_)) aoBlurPs_ = nullptr;
    if (wants[29].code.empty() || !psOf(wants[29], aoApplyPs_)) aoApplyPs_ = nullptr;
    if (wants[30].code.empty() || !psOf(wants[30], torchFirePs_)) torchFirePs_ = nullptr;  // 2.5.2
    if (wants[31].code.empty() || !psOf(wants[31], dustPs_)) dustPs_ = nullptr;
    if (wants[32].code.empty() || !psOf(wants[32], bloomDownPs_)) bloomDownPs_ = nullptr;
    if (wants[33].code.empty() || !psOf(wants[33], dofBlurPs_)) dofBlurPs_ = nullptr;
    if (wants[34].code.empty() || !psOf(wants[34], bloomApplyPs_)) bloomApplyPs_ = nullptr;
    if (wants[35].code.empty() || !psOf(wants[35], stepsPs_)) stepsPs_ = nullptr;
    if (wants[36].code.empty() || !psOf(wants[36], smokePs_)) smokePs_ = nullptr;
    if (wants[37].code.empty() || !psOf(wants[37], smokeGlowPs_)) smokeGlowPs_ = nullptr;
    if (wants[38].code.empty() || !psOf(wants[38], lavaPs_)) lavaPs_ = nullptr;
    if (wants[39].code.empty() || !psOf(wants[39], lavaGlowPs_)) lavaGlowPs_ = nullptr;
    if (wants[40].code.empty() || !psOf(wants[40], heatPs_)) heatPs_ = nullptr;  // 2.5.3
    if (wants[41].code.empty() || !psOf(wants[41], rayPs_)) rayPs_ = nullptr;
    if (wants[42].code.empty() || !psOf(wants[42], rayGlowPs_)) rayGlowPs_ = nullptr;
    if (wants[43].code.empty() || !psOf(wants[43], embersPs_)) embersPs_ = nullptr;
    if (wants[44].code.empty() || !psOf(wants[44], floorReflectPs_)) floorReflectPs_ = nullptr;
    if (wants[45].code.empty() || !psOf(wants[45], halfOverPs_)) halfOverPs_ = nullptr;
    if (wants[46].code.empty() || !psOf(wants[46], splashPs_)) splashPs_ = nullptr;
    if (wants[47].code.empty() || !psOf(wants[47], groundLifePs_)) groundLifePs_ = nullptr;  // 2.5.4
    if (wants[48].code.empty() || !psOf(wants[48], grassPs_)) grassPs_ = nullptr;
    if (wants[49].code.empty() || !vsOf(wants[49], crittersVs_)) crittersVs_ = nullptr;
    if (wants[50].code.empty() || !psOf(wants[50], crittersPs_)) crittersPs_ = nullptr;
    if (wants[51].code.empty() || !psOf(wants[51], shaftsPs_)) shaftsPs_ = nullptr;
    if (wants[52].code.empty() || !psOf(wants[52], flarePs_)) flarePs_ = nullptr;
    if (wants[53].code.empty() || !vsOf(wants[53], mirrorVs_)) mirrorVs_ = nullptr;  // 2.5.9
    if (wants[54].code.empty() || !psOf(wants[54], mirrorPs_)) mirrorPs_ = nullptr;
    if (!wants[1].safe && !wants[22].safe) {  // 2.5.16: none where the player's compiler took the draw shader badly
        std::vector<shader_cache::Shader> all;
        for (const Want& w : wants) all.push_back({w.entry, shader_cache::profileOf(w.profile, w.safe), w.code});
        startVariants(cacheFile, key, ps, std::move(all), kept);
    }
    {  // 0.60.1: the shadows' quads (actor_shadows11.cpp): a place, the texture's coordinates, alpha and depth, feet
        const D3D11_INPUT_ELEMENT_DESC layout[4] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        if (FAILED(device_->CreateInputLayout(layout, 4, wants[7].code.data(), wants[7].code.size(), &shadowLayout_)))
            shadowLayout_ = nullptr;
    }
    // FVF 0x1C4 has one UV set: TEXCOORD1 reads the same bytes as TEXCOORD0 (as pipeline.cpp has it).
    const DXGI_FORMAT colour = bgraVertices_ ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    for (int two = 0; two < 2; ++two) {
        const D3D11_INPUT_ELEMENT_DESC layout[5] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, colour, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 1, colour, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, two ? 32u : 24u, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        if (FAILED(device_->CreateInputLayout(layout, 5, wants[0].code.data(), wants[0].code.size(),
                                              &layout_[two]))) {
            error = "the vertices' layout was refused";
            return false;
        }
    }
    if (mirrorVs_) {  // 2.5.9: the mirror's corners (mirror11.cpp) - x y z, u v, the game's colour
        const D3D11_INPUT_ELEMENT_DESC layout[3] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, colour, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0}};
        if (FAILED(device_->CreateInputLayout(layout, 3, wants[53].code.data(), wants[53].code.size(),
                                              &mirrorLayout_)))
            mirrorLayout_ = nullptr;
    }
    return true;
}

bool Renderer11::makeTarget(Target& t, int width, int height, DXGI_FORMAT format) {
    D3D11_TEXTURE2D_DESC d{};
    d.Width = UINT(width);
    d.Height = UINT(height);
    d.MipLevels = d.ArraySize = 1;
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = device_->CreateTexture2D(&d, nullptr, &t.texture);
    if (SUCCEEDED(hr)) hr = device_->CreateRenderTargetView(t.texture, nullptr, &t.rtv);
    if (SUCCEEDED(hr)) hr = device_->CreateShaderResourceView(t.texture, nullptr, &t.srv);
    if (FAILED(hr)) {
        failed("a target", hr);
        dropTarget(t);
        return false;
    }
    t.width = width;
    t.height = height;
    t.format = format;
    return true;
}

void Renderer11::dropTarget(Target& t) {
    drop(t.srv);
    drop(t.rtv);
    drop(t.texture);
    t.width = t.height = 0;
    t.format = DXGI_FORMAT_UNKNOWN;
}

bool Renderer11::copyScene() {
    if (scene_.width != target_.width || scene_.height != target_.height || scene_.format != target_.format) {
        dropTarget(scene_);
        if (!makeTarget(scene_, target_.width, target_.height, target_.format)) return false;
    }
    ctx_->CopyResource(scene_.texture, target_.texture);
    return true;
}

bool Renderer11::makeDepth(int width, int height, ID3D11Texture2D*& depth, ID3D11DepthStencilView*& dsv,
                           bool sixteen) {
    D3D11_TEXTURE2D_DESC d{};
    d.Width = UINT(width);
    d.Height = UINT(height);
    d.MipLevels = d.ArraySize = 1;
    // 0.60.1: readable too (the characters' shadows read the ground's depth): typeless, a D32 view to draw with
    d.Format = sixteen ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_R32_TYPELESS;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = device_->CreateTexture2D(&d, nullptr, &depth);
    D3D11_DEPTH_STENCIL_VIEW_DESC v{};
    v.Format = sixteen ? DXGI_FORMAT_D16_UNORM : DXGI_FORMAT_D32_FLOAT;
    v.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (SUCCEEDED(hr)) hr = device_->CreateDepthStencilView(depth, &v, &dsv);
    if (FAILED(hr)) {
        failed("a depth buffer", hr);
        drop(dsv);
        drop(depth);
        return false;
    }
    return true;
}

bool Renderer11::makeStaging(int width, int height) {
    drop(staging_);
    D3D11_TEXTURE2D_DESC d{};
    d.Width = UINT(width);
    d.Height = UINT(height);
    d.MipLevels = d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_STAGING;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    const HRESULT hr = device_->CreateTexture2D(&d, nullptr, &staging_);
    if (FAILED(hr)) failed("the frame's read-back copy", hr);
    return SUCCEEDED(hr);
}

bool Renderer11::setTarget(int width, int height) {
    if (lost_ || width <= 0 || height <= 0) return false;
    if (width == gameWidth_ && height == gameHeight_ && target_.texture) return true;
    exitWorld();  // 2.5.16: the picture's own set back, the world's of the old size gone
    dropWorld();
    scaled_ = false;  // the start-up calibration draws at the game's size again
    ctx_->ClearState();
    targetBound_ = false;
    dropTarget(target_);
    dropTarget(game_);
    drop(dsv_);
    drop(depth_);
    gameWidth_ = gameHeight_ = 0;
    if (!makeTarget(target_, width, height) || !makeDepth(width, height, depth_, dsv_) ||
        !makeTarget(game_, width, height) || !makeStaging(width, height)) {
        dropTarget(target_);
        dropTarget(game_);
        drop(dsv_);
        drop(depth_);
        return false;
    }
    gameWidth_ = width;
    gameHeight_ = height;
    scale_ = 1.0;  // until the first present (stage 3 spec §3): the start-up calibration sees DX7's picture
    gameViewport_ = ViewportCmd{Rect{0, 0, width, height}, 0.0f, 1.0f};
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ctx_->ClearRenderTargetView(target_.rtv, black);
    ctx_->ClearDepthStencilView(dsv_, D3D11_CLEAR_DEPTH, 1.0f, 0);
    LOG_INFO("render target %dx%d (Direct3D 11)", width, height);
    return true;
}

bool Renderer11::setScale(double k, int targetWidth, int targetHeight) {
    if (lost_ || !target_.texture || targetWidth <= 0 || targetHeight <= 0) return false;
    exitWorld();  // 2.5.16: the picture's own set (the frame's world made larger into it) before its size changes
    if (k == scale_ && targetWidth == target_.width && targetHeight == target_.height) return true;
    Target next;
    ID3D11Texture2D* depth = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    if (!makeTarget(next, targetWidth, targetHeight) || !makeDepth(targetWidth, targetHeight, depth, dsv)) {
        dropTarget(next);
        return false;
    }
    // The back buffer keeps its picture across a present: the old target is stretched into the new one.
    const float all[4] = {0.0f, 0.0f, float(target_.width), float(target_.height)};
    blit(target_.srv, target_.width, target_.height, all, next.rtv, Rect{0, 0, targetWidth, targetHeight},
         targetWidth, targetHeight, -1.0f);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    dropTarget(target_);
    drop(dsv_);
    drop(depth_);
    target_ = next;
    depth_ = depth;
    dsv_ = dsv;
    scale_ = k;
    targetBound_ = false;
    ctx_->ClearDepthStencilView(dsv_, D3D11_CLEAR_DEPTH, 1.0f, 0);
    LOG_DEBUG("render target %dx%d: %.3f times the game's %dx%d (Direct3D 11)", targetWidth, targetHeight, k,
              gameWidth_, gameHeight_);
    return true;
}

uint32_t Renderer11::createTexture(int width, int height, int levels) {
    if (!device_ || width <= 0 || height <= 0) return 0;
    Texture t;
    t.width = width;
    t.height = height;
    t.levels = std::max(1, levels);
    for (size_t i = 0; i < pool_.size(); ++i) {  // 0.89: one the game let go of this size and levels
        const Pooled& p = pool_[i];
        if (p.width != width || p.height != height || p.levels != t.levels) continue;
        t.texture = p.texture;
        t.srv = p.srv;
        pooledBytes_ -= textureBytes(width, height, t.levels);
        pool_.erase(pool_.begin() + long(i));
        ++stats_.reused;
        const uint32_t id = nextTexture_++;
        textures_.emplace(id, t);
        return id;
    }
    ++stats_.made;
    D3D11_TEXTURE2D_DESC d{};
    d.Width = UINT(width);
    d.Height = UINT(height);
    d.MipLevels = UINT(t.levels);
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = device_->CreateTexture2D(&d, nullptr, &t.texture);
    if (SUCCEEDED(hr)) hr = device_->CreateShaderResourceView(t.texture, nullptr, &t.srv);
    if (FAILED(hr)) {
        failed("a texture", hr);
        drop(t.srv);
        drop(t.texture);
        return 0;
    }
    const uint32_t id = nextTexture_++;
    textures_.emplace(id, t);
    return id;
}

bool Renderer11::createTextureAs(uint32_t id, int width, int height, int levels) {
    if (!id || textures_.count(id)) return false;
    const uint32_t made = createTexture(width, height, levels);
    if (!made) return false;
    const auto it = textures_.find(made);
    Texture t = std::move(it->second);
    textures_.erase(it);
    textures_.emplace(id, std::move(t));
    return true;
}

ID3D11ShaderResourceView* Renderer11::flatView(Texture& t) {
    const int level = light::flatLevel(t.width, t.height, t.levels, t.transparent);
    if (level <= 0 || !t.texture) return nullptr;
    if (t.flat && t.flatLevel == level) return t.flat;
    drop(t.flat);
    D3D11_SHADER_RESOURCE_VIEW_DESC v{};
    v.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    v.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    v.Texture2D.MostDetailedMip = UINT(level);
    v.Texture2D.MipLevels = UINT(t.levels - level);
    if (FAILED(device_->CreateShaderResourceView(t.texture, &v, &t.flat))) t.flat = nullptr;
    t.flatLevel = level;
    return t.flat;
}

void Renderer11::releaseTexture(uint32_t id) {  // Direct3D 11 keeps it while queued work still reads it
    const auto it = textures_.find(id);
    if (it == textures_.end()) return;
    if (ai_) ai_->forget(id);
    objectTextures_.forget(id);  // 0.61
    ++stats_.freed;
    // 0.89: a small one kept for the game's next of its size (32 MB of them at most) - a new surface's levels are
    // uploaded whole before it is first drawn (TextureLevels starts with all of them to upload), the old picture gone
    dropMaterial(it->second);  // 1.14
    drop(it->second.flat);     // 2.5.20
    const Texture& t = it->second;
    const size_t bytes = textureBytes(t.width, t.height, t.levels);
    if (t.width * t.height <= 512 * 512 && pooledBytes_ + bytes <= (size_t(32) << 20) && pool_.size() < 256) {
        pool_.push_back(Pooled{t.texture, t.srv, t.width, t.height, t.levels});
        pooledBytes_ += bytes;
    } else {
        drop(it->second.srv);
        drop(it->second.texture);
    }
    textures_.erase(it);
}

// The record's vertices and indices, once for all its draws - colours turned to R8G8B8A8 where the card takes no
// B8G8R8A8 vertices.
bool Renderer11::fillBuffer(ID3D11Buffer*& buffer, UINT& capacity, UINT bind, const void* data, size_t bytes,
                            const FrameRecord* swizzle) {
    if (!bytes) return true;
    if (bytes > capacity) {
        drop(buffer);
        UINT size = std::max<UINT>(capacity, 1u << 20);
        while (size < bytes) size *= 2;
        D3D11_BUFFER_DESC d{};
        d.ByteWidth = size;
        d.Usage = D3D11_USAGE_DYNAMIC;
        d.BindFlags = bind;
        d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        const HRESULT hr = device_->CreateBuffer(&d, nullptr, &buffer);
        if (FAILED(hr)) {
            failed("a geometry buffer", hr);
            capacity = 0;
            return false;
        }
        capacity = size;
    }
    D3D11_MAPPED_SUBRESOURCE m{};
    const HRESULT hr = ctx_->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    if (FAILED(hr)) {
        failed("writing the geometry", hr);
        return false;
    }
    std::memcpy(m.pData, data, bytes);
    if (swizzle) {
        auto* v = static_cast<uint8_t*>(m.pData);
        for (const DrawCmd& d : swizzle->draws()) {
            const size_t stride = FrameRecord::stride(d.fvf);
            for (uint32_t i = 0; i < d.vertexCount && d.vertexOffset + (i + 1) * stride <= bytes; ++i)
                for (size_t at : {size_t(16), size_t(20)})
                    std::swap(v[d.vertexOffset + i * stride + at], v[d.vertexOffset + i * stride + at + 2]);
        }
    }
    ctx_->Unmap(buffer, 0);
    return true;
}

bool Renderer11::uploadGeometry(const FrameRecord& rec) {
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const std::vector<uint16_t>& i = rec.indices();
    return fillBuffer(vertices_, vertexBytes_, D3D11_BIND_VERTEX_BUFFER, v.data(), v.size(),
                      bgraVertices_ ? nullptr : &rec) &&
           fillBuffer(indices_, indexBytes_, D3D11_BIND_INDEX_BUFFER, i.data(), i.size() * sizeof(uint16_t), nullptr);
}

void Renderer11::setConstants(const Constants& c) {
    // 0.88: most of the game's draws in a row share their constants - what the buffer holds already is not written
    if (stateCache_ && constantsKnown_ && std::memcmp(&c, &constantsNow_, sizeof c) == 0) {
        stats_.spared += 2;  // 0.89: a map and an unmap spared
        return;
    }
    stats_.calls += 2;
    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(ctx_->Map(constants_, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        constantsKnown_ = false;
        return;
    }
    std::memcpy(m.pData, &c, sizeof c);
    ctx_->Unmap(constants_, 0);
    constantsNow_ = c;
    constantsKnown_ = true;
}

ID3D11BlendState* Renderer11::blendState(bool enable, uint32_t src, uint32_t dst, uint8_t writeMask) {
    const uint64_t key = uint64_t(enable) | uint64_t(src) << 1 | uint64_t(dst) << 9 | uint64_t(writeMask) << 17;
    const auto it = blends_.find(key);
    if (it != blends_.end()) return it->second;
    D3D11_BLEND_DESC d{};
    D3D11_RENDER_TARGET_BLEND_DESC& b = d.RenderTarget[0];
    b.BlendEnable = enable ? TRUE : FALSE;
    b.SrcBlend = D3D11_BLEND(src);
    b.DestBlend = D3D11_BLEND(dst);
    b.BlendOp = D3D11_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D11_BLEND_ONE;  // the target's alpha is never read (DESTALPHA reads 1): as Direct3D 12's
    b.DestBlendAlpha = D3D11_BLEND_ZERO;
    b.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    b.RenderTargetWriteMask = writeMask;
    ID3D11BlendState* s = nullptr;
    const HRESULT hr = device_->CreateBlendState(&d, &s);
    if (FAILED(hr)) failed("a blend state", hr);
    blends_.emplace(key, s);
    return s;
}

ID3D11DepthStencilState* Renderer11::depthState(bool enable, bool write, uint32_t func) {
    const uint64_t key = uint64_t(enable) | uint64_t(write) << 1 | uint64_t(func) << 2;
    const auto it = depths_.find(key);
    if (it != depths_.end()) return it->second;
    D3D11_DEPTH_STENCIL_DESC d{};
    d.DepthEnable = enable ? TRUE : FALSE;
    d.DepthWriteMask = write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    d.DepthFunc = D3D11_COMPARISON_FUNC(func);
    ID3D11DepthStencilState* s = nullptr;
    const HRESULT hr = device_->CreateDepthStencilState(&d, &s);
    if (FAILED(hr)) failed("a depth state", hr);
    depths_.emplace(key, s);
    return s;
}

ID3D11RasterizerState* Renderer11::rasterState(uint32_t cull, bool behind) {
    const uint32_t key = cull | (behind ? 0x100u : 0u);
    const auto it = rasters_.find(key);
    if (it != rasters_.end()) return it->second;
    D3D11_RASTERIZER_DESC d{};
    d.FillMode = D3D11_FILL_SOLID;
    d.CullMode = D3D11_CULL_MODE(cull);
    d.FrontCounterClockwise = FALSE;
    d.DepthBias = behind ? 256 : 0;  // 2.6.3 (pipeline.cpp): one step of the game's 16-bit Z in the D32
    d.DepthClipEnable = TRUE;
    d.ScissorEnable = TRUE;  // the game's viewport clips its pre-transformed vertices
    ID3D11RasterizerState* s = nullptr;
    const HRESULT hr = device_->CreateRasterizerState(&d, &s);
    if (FAILED(hr)) failed("a rasterizer state", hr);
    rasters_.emplace(key, s);
    return s;
}

ID3D11SamplerState* Renderer11::samplerState(const StageSetup& st, int aniso) {
    const d3dmap::Sampler m = d3dmap::samplerOf(st, aniso);
    const uint64_t key = uint64_t(m.filter) | uint64_t(m.addressU) << 8 | uint64_t(m.addressV) << 12 |
                         uint64_t(m.maxAnisotropy) << 16 | uint64_t(m.mips) << 21 | uint64_t(st.borderColor) << 22;
    const auto it = samplers_.find(key);
    if (it != samplers_.end()) return it->second;
    D3D11_SAMPLER_DESC d{};
    d.Filter = D3D11_FILTER(m.filter);
    d.AddressU = D3D11_TEXTURE_ADDRESS_MODE(m.addressU);
    d.AddressV = D3D11_TEXTURE_ADDRESS_MODE(m.addressV);
    d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    d.MaxAnisotropy = m.maxAnisotropy;
    d.ComparisonFunc = D3D11_COMPARISON_NEVER;
    std::memcpy(d.BorderColor, m.border, sizeof d.BorderColor);
    d.MinLOD = 0.0f;
    d.MaxLOD = m.mips ? D3D11_FLOAT32_MAX : 0.0f;
    ID3D11SamplerState* s = nullptr;
    const HRESULT hr = device_->CreateSamplerState(&d, &s);
    if (FAILED(hr)) failed("a sampler", hr);
    samplers_.emplace(key, s);
    return s;
}

// Pre-transformed vertices carry their screen position: the Direct3D 7 viewport only clips them and gives the depth
// range. So the viewport is the whole target and the game's viewport becomes the scissor rectangle.
void Renderer11::applyViewport() {
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(target_.width), float(target_.height), gameViewport_.minZ,
                            gameViewport_.maxZ};
    ctx_->RSSetViewports(1, &vp);
    const Rect r = scaleRect(gameViewport_.rect, scale_);
    const LONG left = std::max<LONG>(r.left, 0), top = std::max<LONG>(r.top, 0);
    const D3D11_RECT scissor{left, top, std::max<LONG>(left, std::min<LONG>(r.right, target_.width)),
                             std::max<LONG>(top, std::min<LONG>(r.bottom, target_.height))};
    ctx_->RSSetScissorRects(1, &scissor);
}

void Renderer11::bindTarget() {
    if (targetBound_) return;
    bound_.valid = false;  // 0.88: another pass bound its own since
    ID3D11RenderTargetView* rtvs[2] = {target_.rtv, glowActive_ ? glow_.rtv : nullptr};  // 1.0: the glow too
    ctx_->OMSetRenderTargets(glowActive_ ? 2 : 1, rtvs, dsv_);
    applyViewport();
    ctx_->VSSetShader(drawVs_, nullptr, 0);
    ctx_->PSSetShader(drawPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    targetBound_ = true;
}

void Renderer11::blit(ID3D11ShaderResourceView* src, int srcWidth, int srcHeight, const float srcRect[4],
                      ID3D11RenderTargetView* dst, const Rect& dstRect, int dstWidth, int dstHeight, float sharpen) {
    if (!src || !dst || dstRect.empty() || srcWidth <= 0 || srcHeight <= 0) return;
    targetBound_ = false;  // the game's draws bind theirs again
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 2, none);
    ctx_->OMSetRenderTargets(1, &dst, nullptr);
    const D3D11_VIEWPORT vp{float(dstRect.left), float(dstRect.top), float(dstRect.width()), float(dstRect.height()),
                            0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    const D3D11_RECT clip{std::max<LONG>(dstRect.left, 0), std::max<LONG>(dstRect.top, 0),
                          std::min<LONG>(dstRect.right, dstWidth), std::min<LONG>(dstRect.bottom, dstHeight)};
    ctx_->RSSetScissorRects(1, &clip);
    Constants c{};
    c.sourceRect[0] = srcRect[0] / float(srcWidth);
    c.sourceRect[1] = srcRect[1] / float(srcHeight);
    c.sourceRect[2] = srcRect[2] / float(srcWidth);
    c.sourceRect[3] = srcRect[3] / float(srcHeight);
    // No tap from beyond the source rectangle reaches the copy's edges (stage 3 review M6).
    c.sourceClamp[0] = (srcRect[0] + 0.5f) / float(srcWidth);
    c.sourceClamp[1] = (srcRect[1] + 0.5f) / float(srcHeight);
    c.sourceClamp[2] = std::max(srcRect[0] + 0.5f, srcRect[2] - 0.5f) / float(srcWidth);
    c.sourceClamp[3] = std::max(srcRect[1] + 0.5f, srcRect[3] - 0.5f) / float(srcHeight);
    c.footprint = std::max((srcRect[2] - srcRect[0]) / float(dstRect.width()),
                           (srcRect[3] - srcRect[1]) / float(dstRect.height()));
    c.sharpen = sharpen;
    setConstants(c);
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    ID3D11SamplerState* sampler = samplerState(linearClamp, 0);
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO, D3D11_COLOR_WRITE_ENABLE_ALL), nullptr,
                          0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->PSSetShader(presentPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetShaderResources(0, 1, &src);
    ctx_->PSSetSamplers(0, 1, &sampler);
    ctx_->Draw(3, 0);
    ctx_->PSSetShaderResources(0, 2, none);  // the source may be drawn into next
}

void Renderer11::clear(const ClearCmd& c) {
    const Rect t = scaleRect(c.rect, scale_);  // game pixels -> target pixels
    const Rect r{std::max(t.left, 0), std::max(t.top, 0), std::min(t.right, target_.width),
                 std::min(t.bottom, target_.height)};
    if (r.empty() || !(c.flags & 3)) return;
    const float colour[4] = {float((c.argb >> 16) & 255) / 255.0f, float((c.argb >> 8) & 255) / 255.0f,
                             float(c.argb & 255) / 255.0f, 1.0f};
    if (r.left == 0 && r.top == 0 && r.right == target_.width && r.bottom == target_.height) {
        if (c.flags & 1) ctx_->ClearRenderTargetView(target_.rtv, colour);  // D3DCLEAR_TARGET
        if (c.flags & 2) ctx_->ClearDepthStencilView(dsv_, D3D11_CLEAR_DEPTH, c.z, 0);  // D3DCLEAR_ZBUFFER
        return;
    }
    // A part: a triangle over the target, clipped to it, writing the colour and (or) the depth.
    bindTarget();
    targetBound_ = false;  // its shaders and constants are the clear's for now
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(target_.width), float(target_.height), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    const D3D11_RECT scissor{r.left, r.top, r.right, r.bottom};
    ctx_->RSSetScissorRects(1, &scissor);
    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(ctx_->Map(clearConstants_, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    ClearConstants k{};
    std::memcpy(k.color, colour, sizeof colour);
    k.z = c.z;
    std::memcpy(m.pData, &k, sizeof k);
    ctx_->Unmap(clearConstants_, 0);
    const bool depth = (c.flags & 2) != 0;
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO,
                                     (c.flags & 1) ? D3D11_COLOR_WRITE_ENABLE_ALL : 0),
                          nullptr, 0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(depth, depth, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(clearVs_, nullptr, 0);
    ctx_->PSSetShader(clearPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &clearConstants_);
    ctx_->PSSetConstantBuffers(0, 1, &clearConstants_);
    ctx_->Draw(3, 0);
}

void Renderer11::upload(const FrameRecord& rec, const UploadCmd& u) {
    const auto it = textures_.find(u.texture);
    if (it == textures_.end() || u.rect.empty()) return;
    const Texture& t = it->second;
    const int level = int(u.level);
    const int w = std::max(1, t.width >> level), h = std::max(1, t.height >> level);
    if (level >= t.levels || u.rect.left < 0 || u.rect.top < 0 || u.rect.right > w || u.rect.bottom > h) return;
    const D3D11_BOX box = boxOf(u.rect);
    ctx_->UpdateSubresource(t.texture, UINT(level), &box, rec.pixels().data() + u.pixels, UINT(u.rect.width()) * 4,
                            0);
    stats_.uploaded += uint64_t(u.rect.width()) * uint64_t(u.rect.height()) * 4;  // 0.89
    if (level == 0) {  // 2.5.20: super-light's flat textures keep the shape of one with transparent texels
        const uint32_t* px = rec.pixels().data() + u.pixels;
        const size_t n = size_t(u.rect.width()) * size_t(u.rect.height());
        uint32_t all = 0xFFFFFFFFu;  // every texel ANDed: its alpha 255 only if each one's is (no branch a texel)
        for (size_t i = 0; i < n; ++i) all &= px[i];
        const bool clear = (all >> 24) != 0xFF;
        const bool whole = u.rect.left == 0 && u.rect.top == 0 && u.rect.right == t.width && u.rect.bottom == t.height;
        it->second.transparent = clear || (!whole && it->second.transparent);
    }
    if (level == 0 && u.rect.left == 0 && u.rect.top == 0 && u.rect.right == t.width && u.rect.bottom == t.height) {
        it->second.leafy = foliage::leafyTexture(rec.pixels().data() + u.pixels, t.width, t.height,  // 0.64
                                                 u.slotCount > 0 || !it->second.cells.empty());  // 2.6.19
        // 2.5.1: a whole picture - how much it looks like fire, or lights in its own colour (as device.cpp upload)
        it->second.fire = effect_light::fireOf(rec.pixels().data() + u.pixels, t.width, t.height,
                                               it->second.fireColour);
        it->second.light = effect_light::lightOf(rec.pixels().data() + u.pixels, t.width, t.height,
                                                 it->second.lightColour);
    }
    if (u.slotCount) spriteCellsWritten(it->second.cells, rec.slots().data() + u.firstSlot, u.slotCount);  // 0.60.1
    if (ai_) {  // 0.47: its key, or the sprite cache's new frames with their sources
        std::vector<uint32_t> game;  // 2.5.18: the texels the pack knows it by
        ai_->noteUpload(u.texture, t.width, t.height, u, full_colour::aiTexels(u, rec.pixels().data() + u.pixels, game),
                        u.slotCount ? rec.slots().data() + u.firstSlot : nullptr, u.slotCount,
                        rec.pixels().data());  // 2.5.23: a dyed frame's own colours
    }
}

void Renderer11::writeTarget(const FrameRecord& rec, const WriteTargetCmd& w) {
    const Rect r = w.rect;
    if (r.empty() || r.left < 0 || r.top < 0 || r.right > gameWidth_ || r.bottom > gameHeight_) return;
    const D3D11_BOX box = boxOf(r);
    const uint32_t* px = rec.pixels().data() + w.pixels;
    if (target_.width == gameWidth_ && target_.height == gameHeight_) {  // the game's size: straight in
        ctx_->UpdateSubresource(target_.texture, 0, &box, px, UINT(r.width()) * 4, 0);
        return;
    }
    // A larger target (stage 3 spec §5): the game's pixels into the helper of its size, stretched in.
    ctx_->UpdateSubresource(game_.texture, 0, &box, px, UINT(r.width()) * 4, 0);
    const float from[4] = {float(r.left), float(r.top), float(r.right), float(r.bottom)};
    blit(game_.srv, gameWidth_, gameHeight_, from, target_.rtv, scaleRect(r, scale_), target_.width, target_.height,
         -1.0f);
}

void Renderer11::draw(const FrameRecord& rec, const DrawCmd& d) {
    if (d.vertexCount == 0 || !geometry_) return;
    if (d.primitive != d3d7::PT_TRIANGLELIST && d.primitive != d3d7::PT_TRIANGLESTRIP) {
        static bool logged = false;  // the client only draws lists and strips (states.md §4)
        if (!logged) LOG_INFO("primitive type %u not drawn", static_cast<unsigned>(d.primitive));
        logged = true;
        return;
    }
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + size_t(d.vertexCount) * stride > rec.vertexBytes().size() ||
        (d.indexCount && size_t(d.firstIndex) + d.indexCount > rec.indices().size()))
        return;
    const DrawSetup& s = rec.states()[d.state];
    // 2.6.18: blindness's darkness waits for the world's effects (applyDarkness)
    if (!seamPass_ && !darknessPass_ && !s.ui && d.effect == uint8_t(effect_exe::Kind::Darkness) && !darknessLaid_) {
        noteDarkness(rec, d, s);
        return;
    }
    timeDraws(s.ui, true);  // 2.5.15: the world's and the interface's spans on the card
    // 0.59: the colours over the world, before the interface's first draw (as Direct3D 12's applyGrade); 0.61: the
    // torches' light before them
    // 0.90: a pass of the driver's own draws over what the game drew so far: the gathered call goes first
    if (s.ui && worldDrawn_ && (!stepsDone_ || !nightDone_ || !lightDone_ || !floorDone_ || !mistDone_ ||
                                !skyDone_ || !shaftsDone_ || !flareDone_ || !heatDone_ || !bloomDone_ ||
                                !gradeDone_ || !weatherDone_ || !darknessLaid_))
        flushBatch();
    if (s.ui && worldDrawn_ && !stepsDone_) applySteps();  // 2.5.2: on the ground, under the night
    if (s.ui && worldDrawn_ && !nightDone_) applyNight();  // 0.63: the lights shine out of it
    if (s.ui && worldDrawn_ && !lightDone_) {
        applyTorchLight();
        applyEmbers();    // 2.5.3: the skills' sparks about the places it gathered
        applyCritters();  // 2.5.4: the living world, the embers shining out of the night
    }
    if (s.ui && worldDrawn_ && !floorDone_) applyFloorReflections();  // 2.5.3: lit, before the mists
    if (s.ui && worldDrawn_ && !mistDone_) applyMist();  // 2.5: over the ground, before the colours
    if (s.ui && worldDrawn_ && !skyDone_) applySky();    // 2.5: past the map's edge
    if (s.ui && worldDrawn_ && !shaftsDone_) applyShafts();  // 2.5.5: the light in the air
    if (s.ui && worldDrawn_ && !flareDone_) applyFlare();    // 2.5.5: the sun's flare
    if (s.ui && worldDrawn_ && !heatDone_) applyHeat();  // 2.5.3: the hot air over all of that
    if (s.ui && worldDrawn_ && !bloomDone_) applyBloom();  // 2.5.2: before the colours (ao.cpp finishWorld)
    if (s.ui && worldDrawn_ && !gradeDone_) applyGrade();
    if (s.ui && worldDrawn_ && !flies_.empty()) applyFireflies();  // 1.0.1: with the torches' light
    if (s.ui && worldDrawn_ && !weatherDone_) applyWeather();  // 2.5: in front of all of it
    if (s.ui && !darknessLaid_) applyDarkness();  // 2.6.18: blindness's darkness over all of it
    if (s.ui && glowDrawn_) addGlow();  // 1.0: the world's glow onto it, before the interface
    if (s.ui) exitWorld();              // 2.5.16: the world made larger into the picture, the interface sharp
    if (s.ui) timeDraws(true, false);   // 2.5.15: the interface's span after the world's effects
    // 1.0: from the world's first draw that adds light, its draws write into the glow too
    if (!s.ui && s.blend && s.dstBlend == d3d7::BLEND_ONE && !glowActive_ && glowWanted()) startGlow();
    const bool toGlow = !s.ui && glowActive_ && d.effect != uint8_t(effect_exe::Kind::Aura);  // 1.25
    // 2.5.2: a torch's layers - the glow and (torch_flame) the flame drawn anew once the world is (takeTorch)
    if (!seamPass_ && !s.ui && d.vertexCount == 4 && !objectTextures_.empty() && effects_.torch && takeTorch(rec, d))
        return;
    // 2.6.12: the torches off, the game's flames (drawn as it has them) still show the embers where the fires are
    if (!seamPass_ && !s.ui && d.vertexCount == 4 && !objectTextures_.empty() && !effects_.torch && crittersNeedFire())
        noteCritterFlame(rec, d);
    // 2.5.3: a portal's draw - its middle, for its light and its swirl once the world is drawn (as draw.cpp 0.38)
    if (!s.ui && d.effect == uint8_t(effect_exe::Kind::Portal) && portalsWanted()) notePortal(rec, d);
    // 2.5.2: a character's round shadow - its feet followed for the footprints and the ripples (as draw.cpp)
    if (!s.ui && d.effect == uint8_t(effect_exe::Kind::Shadow) && (stepKind() || ripplesWanted() || crittersNeedFeet()))
        noteFoot(rec, d);
    // 2.5.1: the fire of skills (as draw.cpp): an added picture that looks like fire, or one that lights in its own
    // colour - not the maps' objects (their own)
    if (!s.ui && s.blend && s.dstBlend == d3d7::BLEND_ONE && fireWanted() && d.texture[0]) {
        const auto ft = textures_.find(d.texture[0]);
        const bool mine = d.effect == uint8_t(effect_exe::Kind::None) || d.effect == uint8_t(effect_exe::Kind::Skill);
        if (ft != textures_.end() && ft->second.fire > 0.15f && mine) {
            noteFire(rec, d, ft->second.fire, ft->second.fireColour);
        } else if (ft != textures_.end() && effects_.skillColours && ft->second.light > 0.2f && mine &&
                   (objectTextures_.empty() || objectTextures_.of(d.texture[0]) == objects::Kind::None)) {
            noteFire(rec, d, ft->second.light, ft->second.lightColour);
        }
    }
    // 1.0.1: a firefly the game draws - drawn anew at the interface instead
    if (!s.ui && d.effect == uint8_t(effect_exe::Kind::Firefly) && firefliesWanted() && takeFirefly(rec, d)) return;
    if (!s.ui) worldDrawn_ = true;
    // 0.60.1: the characters' shadows, before the first blended draw after the ground (as draw.cpp does)
    if (!s.ui && d.fvf == d3d7::FVF_TL2) groundDrawn_ = true;
    // 2.5.4: the grass's next cells; 2.5.9 the next frame's smooth hills, this frame's seams
    if (!seamPass_ && !s.ui && d.fvf == d3d7::FVF_TL2 && s.zWrite && (grassWanted() || hillsWanted() || seamsWanted()))
        noteGround(rec, d);
    // 2.5.9 (draw.cpp 1.16): an opaque draw of the world into the next frame's mirror
    if (!seamPass_ && !s.ui && !s.blend && s.zEnable && s.zWrite && mirrorCollecting()) noteCaster(rec, d, s);
    // 2.5.9 (draw.cpp 1.9): the seams' strips at the first blended draw of the world, before the soft shadows - drawn
    // by this function (seamPass_), which makes no notes and no passes for them
    if (!seamPass_ && !s.ui && s.blend && !seamsDone_ && seamsWanted() && !groundTris_.empty()) {
        flushBatch();
        applySeams();
    }
    const bool passes = !seamPass_ && !s.ui && s.blend && groundDrawn_;  // the passes before the characters
    if (passes && !aoDone_ && aoWanted()) {  // 2.5.1: before the characters (draw.cpp)
        flushBatch();
        applyAo();
    }
    if (passes && !groundLifeDone_ && groundLifeWanted() && cameraValid_) {  // 2.5.4
        flushBatch();
        applyGroundLife();
    }
    if (passes && !grassDone_ && grassWanted()) {  // 2.5.4: under the characters
        flushBatch();
        applyGrass();
    }
    if (passes && !shadowsDone_ && shadowsWanted()) {
        flushBatch();  // 0.90
        applyActorShadows(rec, size_t(&d - rec.draws().data()));
    }
    bindTarget();
    // Stage 6: a window tile whose twin was made at this scale is drawn from the twin, a texel per target pixel.
    const bool twin = d.twin && s.ui && twinMatches(d.twinScale, scale_) && textures_.count(d.twin);
    const Texture* t[2] = {&white_, &white_};
    for (int i = 0; i < 2; ++i) {
        const uint32_t id = i == 0 && twin ? d.twin : d.texture[i];
        const auto it = id ? textures_.find(id) : textures_.end();
        if (it != textures_.end()) t[i] = &it->second;
    }
    // 0.47: AI textures - a texture with a picture of its own is drawn from it (the world's; the interface's big
    // pictures), its mip levels sampled also where the game uses none: four times the texels would shimmer.
    ID3D11ShaderResourceView* srvs[2] = {t[0]->srv, t[1]->srv};
    StageSetup sampled[2] = {s.stage[0], s.stage[1]};
    bool aiDrawn[2] = {false, false};
    const bool aiHere = !s.ui || aiInterfaceAt(scale_);  // 0.89: not the interface at the game's own size
    if (ai_ && aiHere) {
        for (int i = 0; i < 2 && !(d.twin && s.ui); ++i) {
            // 2.2.2: a sprite cache's by the kind of the cell drawn
            const uint8_t source = t[i]->cells.empty() ? 0
                                                       : spriteSourceOf(t[i]->cells, t[i]->width, t[i]->height,
                                                                        rec.vertexBytes(), d.vertexOffset, stride,
                                                                        d.vertexCount);
            ID3D11ShaderResourceView* copy = t[i] != &white_ ? ai_->copyOf(d.texture[i], s.ui, source) : nullptr;
            if (!copy) continue;
            srvs[i] = copy;
            aiDrawn[i] = true;
            if (sampled[i].mipFilter != d3d7::TFP_POINT && sampled[i].mipFilter != d3d7::TFP_LINEAR)
                sampled[i].mipFilter = d3d7::TFP_LINEAR;
        }
        if (!s.ui) {  // what the draw shows: the ground, its lightmap, a sprite, an effect, the rest of the world
            ai::DrawFacts facts;
            facts.uvSets = d.fvf == d3d7::FVF_TL2 ? 2 : 1;
            facts.additive = s.blend && s.dstBlend == d3d7::BLEND_ONE;
            facts.flatQuad = d.vertexCount == 4 && ai::flatQuad(rec.vertexBytes().data() + d.vertexOffset, stride,
                                                                d.vertexCount, d.indexCount);
            for (int i = 0; i < 2; ++i) {
                if (!d.texture[i] || t[i] == &white_) continue;
                facts.stage = i;
                ai_->noteWorldUse(d.texture[i], ai::drawKind(facts));
            }
        } else if (!twin && d.texture[0] && t[0] != &white_) {
            ai_->noteUiUse(d.texture[0]);
        }
    }
    // 2.5.20: super-light - the ground's and the models' textures as their colour alone (light::kFlatWorld); the
    // sprites, the effects and the interface as they are
    if ((light_ & light::kFlatWorld) && !s.ui && !twin && d.texture[0] && t[0] != &white_) {
        ai::DrawFacts facts;
        facts.uvSets = d.fvf == d3d7::FVF_TL2 ? 2 : 1;
        facts.additive = s.blend && s.dstBlend == d3d7::BLEND_ONE;
        facts.flatQuad = d.vertexCount == 4 && ai::flatQuad(rec.vertexBytes().data() + d.vertexOffset, stride,
                                                            d.vertexCount, d.indexCount);
        const ai::Kind kind = ai::drawKind(facts);
        const auto it = textures_.find(d.texture[0]);
        if ((kind == ai::Kind::Ground || kind == ai::Kind::Objects) && it != textures_.end())
            if (ID3D11ShaderResourceView* flat = flatView(it->second)) {
                srvs[0] = flat;
                aiDrawn[0] = false;
            }
    }
    Constants c{};
    c.targetSize[0] = float(target_.width);
    c.targetSize[1] = float(target_.height);
    packStage(s.stage[0], d.keyed[0], c.stage0);
    packStage(s.stage[1], d.keyed[1], c.stage1);
    // 2.5.2: the ground's lightmap (its 256x256 atlas of 8x8 cells) sampled smoothly (as draw.cpp, 0.22.6) - the
    // weak set's own, never set here until now
    if (effects_.smoothLight && !s.ui && d.fvf == d3d7::FVF_TL2 && t[1]->width == 256 && t[1]->height == 256)
        c.stage1[1] |= 1u << 25;
    c.alphaTest = s.alphaTest ? uint32_t(s.alphaFunc) | uint32_t(s.alphaRef) << 8 | 1u << 16 : 0u;
    c.fogEnable = gameFogDrawn(s.fog, effects_.noGameFog, outdoors_) ? 1u : 0u;  // 2.6.10
    c.fogColor[0] = float((s.fogColor >> 16) & 255) / 255.0f;
    c.fogColor[1] = float((s.fogColor >> 8) & 255) / 255.0f;
    c.fogColor[2] = float(s.fogColor & 255) / 255.0f;
    c.fogColor[3] = 1.0f;
    c.colorKey = s.colorKey ? 1u : 0u;
    c.uiMode = uint32_t(uiFilterMode(s.ui, scale_, uiSharp_ ? UiFilter::Sharp : UiFilter::Smooth));
    if (aiDrawn[0]) c.uiMode = 0;  // the AI's picture has the detail the game's pixels lack: sampled as it is
    if (twin) {  // the tile's first game pixel is the window's place plus the tile's (spec §6)
        float quad[4];
        uiPixels(rec, d, quad);
        c.uiMode = 3;
        c.twin[0] = float(tileOrigin(int(quad[0]) - d.twinTile[0], d.twinTile[0], scale_, d.twinScale));
        c.twin[1] = float(tileOrigin(int(quad[1]) - d.twinTile[1], d.twinTile[1], scale_, d.twinScale));
        c.twin[2] = float(t[0]->width);
        c.twin[3] = float(t[0]->height);
    } else if (c.uiMode != 0) {
        uiPixels(rec, d, c.uiPixels);
    }
    c.scale = float(scale_);
    if (!s.ui && d.fvf == d3d7::FVF_TL2 && c.uiMode == 0 && effects_.relief && effects_.reliefStrength > 0) {
        c.stage1[1] |= 1u << 26;  // 0.71: the ground's relief (as draw.cpp)
        c.uiPixels[0] = float(std::clamp(effects_.reliefStrength, 0, 200)) / 100.0f;
        c.uiPixels[1] = t[0] ? float(t[0]->width) : 0.0f;  // 1.5.10: the game's texels, also under an AI picture
        // 2.6.7 (draw.cpp): the projection for shade()'s seen - else 0 without the materials, the hills, the seams
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
    }
    // 2.6.19 (draw.cpp): leaves as the game draws a tree's - alpha-tested, not blended: never a sprite or an effect
    const bool leaves = !s.ui && !twin && c.uiMode == 0 && t[0] && t[0]->leafy && d.fvf != d3d7::FVF_TL2 && !s.blend;
    float swayTop = 0.0f, swayBottom = 0.0f;  // 0.64: leaves in the wind (as draw.cpp)
    if (leaves && effects_.foliage && effects_.foliageStrength > 0 && yExtent(rec, d, swayTop, swayBottom)) {
        const unsigned long long now = GetTickCount64();
        if (!clockStart_) clockStart_ = now;
        c.twin[0] = 1.2f * float(effects_.foliageStrength) / 100.0f;
        c.twin[1] = swayBottom;
        c.twin[2] = swayBottom - swayTop;
        c.twin[3] = float(double(now - clockStart_) / 1000.0);
    }
    if (leaves) {  // 1.7: the trees
        LeafLook l;
        l.volume = effects_.treeVolume;
        l.through = effects_.leafLight;
        l.tints = effects_.treeTints;
        l.gusts = effects_.gusts && effects_.foliage && effects_.foliageStrength > 0;
        const daylight::Light sun = daylight::at(effects_.dayCycle ? effects_.dayNow : -1);
        l.sun[0] = sun.shadow[0];
        l.sun[1] = sun.shadow[1];
        l.throughLight = trees::throughLight(sun.length, float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f);
        l.season = season_;
        l.viewToWorld = cameraValid_ ? viewToWorld_ : nullptr;
        l.projection = camera::projectionFor(gameWidth_, gameHeight_);
        leafShading(rec, d, l, c);
        if (windShared())  // 2.5.5: the leaves sway the shared wind's way
            c.uiPixels[3] *= (windScreenX_ < 0.0f ? -1.0f : 1.0f) * (0.4f + 0.6f * windNow_.strength);
        if (effects_.leafFall) noteCrown(rec, d);  // 2.5.4: the leaves falling from it (the living world)
    }
    // 0.99, the modern water (as draw.cpp): a cell of the map's water by water_vs/water_ps, from the picture and the
    // depth before the frame's first water; without them as the game draws it.
    bool water = false;
    if (!s.ui && effects_.water && waterVs_ && waterPs_ && cameraValid_ && d.fvf == d3d7::FVF_TL1 &&
        d.vertexCount == 4 && s.blend && s.srcBlend == d3d7::BLEND_SRCALPHA &&
        s.dstBlend == d3d7::BLEND_INVSRCALPHA &&
        water::isWaterQuad(rec.vertexBytes().data() + d.vertexOffset, stride, d.vertexCount, d.indexCount,
                           d.primitive == d3d7::PT_TRIANGLESTRIP))
        water = (!toGlow || waterGlowPs_) && copyForWater();  // 1.0: in the glow, its own glow shader
    if (water) noteWaterLevel(rec, d);  // 2.5.4: the wet sand's and the creatures' water level
    if (water && !mirrorDone_ && waterLevelKnown_ && mirrorCollecting() && !mirrorCasters_.empty()) {
        applyMirror();  // 2.5.9 (draw.cpp 1.16): the world mirrored, before the frame's first water
        bindTarget();
    }
    if (water) {  // the game's camera, the waves' time and the page's settings (water_ps)
        std::memcpy(c.reproject, viewToWorld_, sizeof c.reproject);
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        c.sharpen = waterTime();
        c.footprint = float(effects_.waterWaves) / 100.0f;
        c.twin[0] = float(effects_.waterReflections) / 100.0f;
        c.twin[1] = float(effects_.waterClarity) / 100.0f;
        // 1.7: the water's life (water_ps waterLife); 2.5.9 and its parts - the foam, the glints, the light on the
        // bed (Direct3D 12's since 2.4, never set here until now)
        c.twin[3] = waterLife();
        if (mirrorMade_) c.stage0[1] |= 1u << 27;  // 2.5.9 (draw.cpp 1.16): the world mirrored (t5)
        if (ripplesWanted()) {  // 2.5.2 (draw.cpp 0.94): the three newest steps (x, z, age), one in each free vector
            float* slot[3] = {c.uiPixels, c.sourceRect, c.sourceClamp};
            int n = 0;
            const float now = waterTime();
            const std::deque<footsteps::Step>& steps = footTracker_.steps();
            for (auto it = steps.rbegin(); it != steps.rend() && n < 3; ++it) {
                const float age = now - it->born;
                if (age < 0.0f || age > kRippleLife) continue;
                slot[n][0] = it->x;
                slot[n][1] = it->z;
                slot[n][2] = age;
                ++n;
            }
            c.twin[2] = float(n);
        }
        if (!waterCells_++) LOG_INFO("water: the map's water found, drawn anew (Direct3D 11)");
    }
    // 2.5.2: a puff of the chimneys' smoke (smoke_ps, draw.cpp 0.24) and a cell of lava (lava_ps, 0.31) by their own
    // shaders: the camera backwards, the time, the density or the brightness; the lava lights around it
    bool smoke = false, lava = false;
    if (!s.ui && !water && smokeWanted() && d.vertexCount == 4 && !objectTextures_.empty() &&
        objectTextures_.of(d.texture[0]) == objects::Kind::Smoke)
        smoke = d.vertexOffset + 4 * stride <= rec.vertexBytes().size();
    if (!s.ui && !water && !smoke && lavaWanted() && d.fvf == d3d7::FVF_TL1 && d.vertexCount == 4 && s.blend &&
        s.srcBlend == d3d7::BLEND_SRCALPHA && s.dstBlend == d3d7::BLEND_INVSRCALPHA && lavaMap()) {
        lava = d.vertexOffset + 4 * stride <= rec.vertexBytes().size() &&
               water::isLavaQuad(rec.vertexBytes().data() + d.vertexOffset, stride, d.vertexCount, d.indexCount,
                                 d.primitive == d3d7::PT_TRIANGLESTRIP);
        if (lava) noteLava(rec, d);
        if (lava && !lavaLogged_) LOG_INFO("lava: %s's drawn as living lava (Direct3D 11)", map_.c_str());
        lavaLogged_ = lavaLogged_ || lava;
    }
    if (smoke || lava) {
        static const float kSame[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};  // the camera unknown: the view itself
        std::memcpy(c.reproject, cameraValid_ ? viewToWorld_ : kSame, sizeof c.reproject);
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        c.twin[3] = waterTime();
    }
    if (smoke) {  // its depth (the quad's), the density
        float rhw = 0.0f;
        for (size_t k = 0; k < 4; ++k) {
            float w = 0.0f;
            std::memcpy(&w, rec.vertexBytes().data() + d.vertexOffset + k * stride + 12, sizeof w);
            rhw += w * 0.25f;
        }
        c.twin[0] = rhw > 0.0f ? 1.0f / rhw : 1.0f;
        c.footprint = float(std::clamp(effects_.smokeDensity, 0, 200)) / 100.0f;
        if (!smokeLogged_) LOG_INFO("smoke: the chimneys' smoke drawn anew (Direct3D 11)");
        smokeLogged_ = true;
    }
    if (lava) c.footprint = float(std::clamp(effects_.lavaBrightness, 0, 200)) / 100.0f;
    // 2.5.3: a face of one of the forest's beams of light (ray_ps, draw.cpp 0.30): its middle and foot noted for the
    // dust and the light where it falls
    const bool ray = !s.ui && !water && !smoke && !lava && d.effect == uint8_t(effect_exe::Kind::Ray) && raysWanted() &&
                     d.vertexOffset + size_t(d.vertexCount) * stride <= rec.vertexBytes().size();
    if (ray) {
        noteRay(rec, d);
        static const float kSame[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        std::memcpy(c.reproject, cameraValid_ ? viewToWorld_ : kSame, sizeof c.reproject);
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        c.twin[3] = waterTime();
        c.footprint = float(std::clamp(effects_.raysBrightness, 0, 200)) / 100.0f;
        if (!raysLogged_) LOG_INFO("rays: the forest's light drawn anew (Direct3D 11)");
        raysLogged_ = true;
    }
    // 0.67, soft particles (as draw.cpp): an effect facing the screen fades where it goes into the ground; the
    // depth read while it is tested only.
    const bool adds = s.blend && s.dstBlend == d3d7::BLEND_ONE;
    // 1.0: its colour into the glow - light it adds, or a factor it multiplies the light under it by (as draw.cpp)
    if (toGlow && (adds || (s.blend && (s.srcBlend == d3d7::BLEND_ZERO || s.srcBlend == d3d7::BLEND_DESTCOLOR))))
        c.alphaTest |= 1u << 19;
    if (toGlow) glowDrawn_ = true;
    const bool effectDraw = adds || d.effect == uint8_t(effect_exe::Kind::Skill) ||
                            d.effect == uint8_t(effect_exe::Kind::Portal);
    const bool soft = !water && !smoke && !lava && !ray && !s.ui && effects_.softParticles && d.vertexCount == 4 &&
                      s.blend && s.zEnable &&
                      !s.zWrite && effectDraw && s.srcBlend != d3d7::BLEND_ZERO &&
                      s.srcBlend != d3d7::BLEND_DESTCOLOR &&
                      ai::flatQuad(rec.vertexBytes().data() + d.vertexOffset, stride, d.vertexCount, d.indexCount) &&
                      depthViews();
    if (soft) {
        c.alphaTest |= 1u << 20;
        if (s.srcBlend == d3d7::BLEND_ONE) c.alphaTest |= 1u << 21;
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
    }
    // 1.14, the materials (as draw.cpp): a draw of the ground or a model (it writes depth, not a sprite, not leaves,
    // not an effect) - its texture's material made first (materials11.cpp), then read with it (t2).
    ID3D11ShaderResourceView* mat = nullptr;
    if (materialsWanted() && !s.ui && !water && !smoke && !lava && !ray && !soft && !adds && !twin && !d.twin &&
        (s.zWrite || seamPass_) && t[0] != &white_ &&
        !t[0]->leafy && c.uiMode == 0 &&
        !(d.vertexCount == 4 &&
          ai::flatQuad(rec.vertexBytes().data() + d.vertexOffset, stride, d.vertexCount, d.indexCount))) {
        Texture& own = textures_.find(d.texture[0])->second;
        if (ensureMaterial(own, srvs[0])) mat = own.matSrv;
        bindTarget();  // a material made just now bound its own
    }
    if (mat) materialConstants(d.fvf == d3d7::FVF_TL2, c);
    // 2.5.5 (draw.cpp 1.17): a sprite of the game's sprite cache (a character, a monster) facing the screen, blended
    // over - lit by the sun from its silhouette's sides
    if (!mat && effects_.spriteLight && effects_.spriteLightStrength > 0 && !s.ui && !water && !smoke && !lava &&
        !ray && !adds && !d.twin && c.uiMode == 0 && s.blend && d.vertexCount == 4 && t[0] != &white_ &&
        !t[0]->cells.empty() && !t[0]->leafy &&
        ai::flatQuad(rec.vertexBytes().data() + d.vertexOffset, stride, d.vertexCount, d.indexCount))
        spriteConstants(c, t[0]->width);
    // 2.5.9 (draw.cpp 1.15): smooth hills - a draw of the ground (its own or a seam's strip) reads the corners'
    // normals (t3)
    const bool hilly = hillsWanted() && !s.ui && d.fvf == d3d7::FVF_TL2 && !water && !d.twin &&
                       (s.zWrite || seamPass_) && ensureHills();
    if (hilly) hillsConstants(c);
    if (seamPass_) {  // 2.5.9 (draw.cpp 1.9): a strip across a seam - its alpha from the noise over the world
        c.stage1[1] |= 1u << 31;
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        std::memcpy(c.reproject, viewToWorld_, sizeof c.reproject);
    }
    // 0.90: the draw call's state; the draw goes on the gathered call when it is the same and its vertices follow
    Issue is;
    is.c = c;
    const int aniso = s.ui ? 0 : anisotropy_;
    if ((light_ & light::kPointMips) && !s.ui)  // 2.5.20: super-light - one mip level a texel, not two blended
        for (StageSetup& st : sampled)
            if (st.mipFilter == d3d7::TFP_LINEAR) st.mipFilter = d3d7::TFP_POINT;
    is.samplers[0] = samplerState(sampled[0], aniso);
    is.samplers[1] = samplerState(sampled[1], aniso);
    is.blend = blendState(s.blend, d3dmap::blendOf(s.srcBlend, d3d7::BLEND_ONE),
                          d3dmap::blendOf(s.dstBlend, d3d7::BLEND_ZERO),
                          D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                              D3D11_COLOR_WRITE_ENABLE_BLUE);
    is.depth = depthState(s.zEnable, s.zWrite, d3dmap::compareOf(s.zFunc));
    // 2.6.3: a model a step of the game's 16-bit Z behind the ground (pipeline_key.hpp behindGround); the 16-bit
    // world's D16 has that step itself
    is.raster = rasterState(d3dmap::cullOf(s.cull), behindGround(s, d.fvf, groundDrawn_) &&
                                                        !((light_ & light::kWorld16) && world16Ok_));
    is.srvs[0] = srvs[0];
    is.srvs[1] = srvs[1];
    is.srvs[2] = mat;
    is.srvs[3] = hilly ? hillsSrv_ : nullptr;  // 2.5.9
    is.layout = layout_[d.fvf == d3d7::FVF_TL2 ? 1 : 0];
    is.vs = water ? waterVs_ : drawVs_;
    // 2.5: a draw whose constants carry none of the world's extras takes the lean shader (as draw.cpp)
    const bool lean = !water && !smoke && !lava && !ray &&
                      (c.stage1[1] & (1u << 26 | 1u << 27 | 1u << 28 | 1u << 30 | 1u << 31)) == 0 &&
                      (c.stage0[1] & (1u << 26 | 1u << 28)) == 0 && (c.alphaTest & 1u << 20) == 0;
    is.ps = water    ? (toGlow ? waterGlowPs_ : waterPs_)
            : smoke  ? (toGlow ? smokeGlowPs_ : smokePs_)  // 2.5.2
            : lava   ? (toGlow ? lavaGlowPs_ : lavaPs_)
            : ray    ? (toGlow ? rayGlowPs_ : rayPs_)  // 2.5.3
            : toGlow ? (lean && drawGlowLeanPs_ ? drawGlowLeanPs_ : drawGlowPs_)
                     : lean && drawLeanPs_ ? drawLeanPs_ : drawPs_;
    if (!water && !smoke && !lava && !ray) {  // 2.5.16: the shader made for this state, once it is
        using shader_variants::Base;
        const Base base = toGlow ? (lean && drawGlowLeanPs_ ? Base::GlowLean : Base::Glow)
                                 : (lean && drawLeanPs_ ? Base::Lean : Base::Full);
        if (ID3D11PixelShader* own = variantOf(base, is.c)) is.ps = own;
    }
    is.primitive = d.primitive;
    is.firstIndex = d.firstIndex;
    is.indexCount = d.indexCount;
    batch::Draw shape{batch::kindOf(d.primitive, d.vertexCount, d.indexCount != 0), d.vertexOffset, d.vertexCount,
                      uint32_t(stride)};
    // By itself as before: merging off, a soft particle (its depth bound for it alone), quads without the pattern.
    if (!mergeDraws_ || soft || (shape.kind == batch::Kind::Quads && !quadIndices_))
        shape.kind = batch::Kind::Single;
    ++stats_.draws;
    if (batch_.joins(shape) && is.sameState(issue_)) {
        batch_.add(shape);
        return;
    }
    flushBatch();
    ID3D11RenderTargetView* rtvs[2] = {target_.rtv, glowActive_ ? glow_.rtv : nullptr};  // 1.0: and the glow
    const UINT rtvCount = glowActive_ ? 2 : 1;
    if (soft) {  // the depth read, not written, while it is tested
        ctx_->OMSetRenderTargets(rtvCount, rtvs, readOnlyDsv_);
        ctx_->PSSetShaderResources(3, 1, &depthSrv_);
    }
    issue_ = is;
    for (ID3D11ShaderResourceView* v : issue_.srvs)
        if (v) v->AddRef();  // held while gathered: an AI copy put away in the meantime stays alive for it
    batch_.start(shape);
    if (shape.kind == batch::Kind::Single) flushBatch();
    if (soft) {  // the depth written again by the next draws
        ID3D11ShaderResourceView* none = nullptr;
        ctx_->PSSetShaderResources(3, 1, &none);
        ctx_->OMSetRenderTargets(rtvCount, rtvs, dsv_);
    }
}

bool Renderer11::Issue::sameState(const Issue& o) const {
    return vs == o.vs && ps == o.ps && blend == o.blend && depth == o.depth && raster == o.raster &&
           samplers[0] == o.samplers[0] &&
           samplers[1] == o.samplers[1] && srvs[0] == o.srvs[0] && srvs[1] == o.srvs[1] && srvs[2] == o.srvs[2] &&
           srvs[3] == o.srvs[3] && layout == o.layout &&
           std::memcmp(&c, &o.c, sizeof c) == 0;
}

// 0.90: the gathered draws as one draw call - the quads through their index pattern, a list as it is, a draw by
// itself as the game made it. 0.88: only what differs from the last call's is called (bound_).
void Renderer11::flushBatch() {
    if (!batch_.open()) return;
    const Issue& is = issue_;
    const batch::Kind kind = batch_.kind();
    setConstants(is.c);
    const D3D11_PRIMITIVE_TOPOLOGY topology = kind == batch::Kind::Single && is.primitive == d3d7::PT_TRIANGLESTRIP
                                                  ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                                                  : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    const bool all = !bound_.valid || !stateCache_;
    unsigned issued = 1, spared = 0;  // 0.89: counted for the log (the draw itself is one)
    const auto need = [&](bool differs) {
        ++(differs ? issued : spared);
        return differs;
    };
    if (is.ps && (is.ps == waterPs_ || is.ps == waterGlowPs_)) {
        // 0.99: the picture and the depth under the water (t2, t3) - 2.5.9: in the glow too (water_glow_ps read none
        // until now); the world mirrored (t5)
        ID3D11ShaderResourceView* under[2] = {scene_.srv, depthCopySrv_};
        ctx_->PSSetShaderResources(2, 2, under);
        ID3D11ShaderResourceView* mirrored = mirrorMade_ ? mirror_.srv : nullptr;
        ctx_->PSSetShaderResources(5, 1, &mirrored);
    } else {
        if (is.srvs[2]) ctx_->PSSetShaderResources(2, 1, &is.srvs[2]);  // 1.14: the material (t2), with each call
        if (is.srvs[3]) ctx_->PSSetShaderResources(3, 1, &is.srvs[3]);  // 2.5.9: the hills' normals (t3)
    }
    if (need(all || is.vs != bound_.vs)) ctx_->VSSetShader(is.vs, nullptr, 0);  // 0.99: the water's, or the game's
    if (need(all || is.ps != bound_.ps)) ctx_->PSSetShader(is.ps, nullptr, 0);
    if (need(all || is.blend != bound_.blend)) ctx_->OMSetBlendState(is.blend, nullptr, 0xFFFFFFFF);
    if (need(all || is.depth != bound_.depth)) ctx_->OMSetDepthStencilState(is.depth, 0);
    if (need(all || is.raster != bound_.raster)) ctx_->RSSetState(is.raster);
    if (need(all || is.samplers[0] != bound_.samplers[0] || is.samplers[1] != bound_.samplers[1]))
        ctx_->PSSetSamplers(0, 2, is.samplers);
    if (need(all || is.srvs[0] != bound_.srvs[0] || is.srvs[1] != bound_.srvs[1]))
        ctx_->PSSetShaderResources(0, 2, is.srvs);
    if (need(all || is.layout != bound_.layout)) ctx_->IASetInputLayout(is.layout);
    if (need(all || unsigned(topology) != bound_.topology)) ctx_->IASetPrimitiveTopology(topology);
    // The record's vertices lie one draw after another: the buffer stays bound at the phase of the call's first
    // vertex within its stride, the call starting at its vertex number - bound again only when the stride or the
    // phase changes (a draw of the other vertex kind in between).
    const UINT strideBytes = batch_.stride(), phase = batch_.firstByte() % strideBytes,
               first = batch_.firstByte() / strideBytes;
    if (need(all || vertices_ != bound_.vertexBuffer || strideBytes != bound_.stride || phase != bound_.phase))
        ctx_->IASetVertexBuffers(0, 1, &vertices_, &strideBytes, &phase);
    ID3D11Buffer* indices = kind == batch::Kind::Quads ? quadIndices_
                            : kind == batch::Kind::Single && is.indexCount ? indices_
                                                                           : nullptr;
    if (indices) {
        if (need(all || indices != bound_.indexBuffer)) ctx_->IASetIndexBuffer(indices, DXGI_FORMAT_R16_UINT, 0);
        bound_.indexBuffer = indices;
        if (kind == batch::Kind::Quads)
            ctx_->DrawIndexed(batch_.vertices() / 4 * 6, 0, INT(first));
        else
            ctx_->DrawIndexed(is.indexCount, is.firstIndex, INT(first));
    } else {
        if (all) bound_.indexBuffer = nullptr;
        ctx_->Draw(batch_.vertices(), first);
    }
    ++stats_.batches;
    stats_.calls += issued;
    stats_.spared += spared;
    bound_.valid = true;
    bound_.vs = is.vs;
    bound_.ps = is.ps;
    bound_.blend = is.blend;
    bound_.depth = is.depth;
    bound_.raster = is.raster;
    bound_.samplers[0] = is.samplers[0];
    bound_.samplers[1] = is.samplers[1];
    bound_.srvs[0] = is.srvs[0];
    bound_.srvs[1] = is.srvs[1];
    bound_.layout = is.layout;
    bound_.topology = unsigned(topology);
    bound_.vertexBuffer = vertices_;
    bound_.stride = strideBytes;
    bound_.phase = phase;
    for (ID3D11ShaderResourceView* v : issue_.srvs)
        if (v) v->Release();  // bound now: Direct3D holds them
    batch_.close();
}

// 0.99: before the frame's first water - its picture and depth so far copied (once a frame); false: no water anew.
bool Renderer11::copyForWater() {
    if (waterCopied_) return waterReady_;
    waterCopied_ = true;
    waterReady_ = false;
    if (!depth_ || !target_.texture) return false;
    flushBatch();  // the draws before the water go into the copies
    EffectSpan span(*this, kTimeWater11);  // 2.5.9: the copies alone, not the draws before them
    D3D11_TEXTURE2D_DESC dd{};
    depth_->GetDesc(&dd);
    const bool sixteen = dd.Format == DXGI_FORMAT_R16_TYPELESS;  // 2.5.21: the 16-bit world's depth copied as it is
    D3D11_TEXTURE2D_DESC cd{};
    if (depthCopy_) depthCopy_->GetDesc(&cd);
    if (!depthCopy_ || cd.Width != dd.Width || cd.Height != dd.Height || cd.Format != dd.Format) {
        drop(depthCopySrv_);
        drop(depthCopyDsv_);
        drop(depthCopy_);
        if (!makeDepth(int(dd.Width), int(dd.Height), depthCopy_, depthCopyDsv_, sixteen)) return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};
        v.Format = sixteen ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R32_FLOAT;
        v.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        v.Texture2D.MipLevels = 1;
        if (FAILED(device_->CreateShaderResourceView(depthCopy_, &v, &depthCopySrv_))) {
            depthCopySrv_ = nullptr;
            return false;
        }
    }
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx_->PSSetShaderResources(2, 2, none);  // neither copy bound while it is written
    if (!copyScene()) return false;
    ctx_->CopyResource(depthCopy_, depth_);
    waterReady_ = true;
    return true;
}

// 2.5.9 (draw.cpp waterLife): water_ps's bits - 2 a swamp's bubbles, 4 the surf's swash (1, the fish, gone in 2.7.2);
// the water's own parts, each a box of its own since 2.4: 8 the foam along the shore, 16 the sun's glints, 32 the
// light on the bed
float Renderer11::waterLife() const {
    return float((effects_.swamp && place_.swamp ? 2 : 0) +
                 (effects_.shoreFoam ? 4 : 0) + (effects_.waterFoam ? 8 : 0) + (effects_.waterGlints ? 16 : 0) +
                 (effects_.caustics ? 32 : 0));
}

float Renderer11::waterTime() {
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    if (!waterClock_.QuadPart) waterClock_ = now;
    return float(double(now.QuadPart - waterClock_.QuadPart) / double(freq.QuadPart));
}

void Renderer11::setCamera(bool valid, const float view[12], float targetZ) {
    float v4[16], inv[16];
    camera::affine4(view, v4);
    cameraValid_ = valid && camera::inverse4(v4, inv);
    cameraTargetZ_ = cameraValid_ ? targetZ : 0.0f;  // 2.5: the low fog's and the land's height
    if (cameraValid_)
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c) viewToWorld_[3 * r + c] = inv[4 * r + c];
}

bool Renderer11::depthViews() {
    if (!depth_) return false;
    D3D11_TEXTURE2D_DESC desc{};
    depth_->GetDesc(&desc);
    const bool sixteen = desc.Format == DXGI_FORMAT_R16_TYPELESS;  // 2.5.20: super-light's world
    if (depthSrvOf_ != depth_) {
        drop(depthSrv_);
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};
        v.Format = sixteen ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R32_FLOAT;
        v.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        v.Texture2D.MipLevels = 1;
        depthSrvOf_ = depth_;
        if (FAILED(device_->CreateShaderResourceView(depth_, &v, &depthSrv_))) depthSrv_ = nullptr;
    }
    if (readOnlyDsvOf_ != depth_) {
        drop(readOnlyDsv_);
        D3D11_DEPTH_STENCIL_VIEW_DESC v{};
        v.Format = sixteen ? DXGI_FORMAT_D16_UNORM : DXGI_FORMAT_D32_FLOAT;
        v.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        v.Flags = D3D11_DSV_READ_ONLY_DEPTH;
        readOnlyDsvOf_ = depth_;
        if (FAILED(device_->CreateDepthStencilView(depth_, &v, &readOnlyDsv_))) readOnlyDsv_ = nullptr;
    }
    return depthSrv_ && readOnlyDsv_;
}

bool Renderer11::ensureWorld() {
    // 2.5.20: super-light's 16-bit world has a set of its own at any scale
    const bool sixteen = (light_ & light::kWorld16) && world16Ok_;
    if (inWorld_ || lost_ || !scaled_ || !target_.texture || (worldScale_ > 0.995 && !sixteen)) return false;
    const double k = scale_ * worldScale_;
    const int w = std::max(16, int(std::lround(gameWidth_ * k))), h = std::max(16, int(std::lround(gameHeight_ * k)));
    if (w > target_.width || h > target_.height || (!sixteen && (w == target_.width || h == target_.height)))
        return false;
    world_.scale = k;
    if (world_.target.texture && world_.target.width == w && world_.target.height == h && world_.sixteen == sixteen)
        return true;
    dropWorld();
    if (!makeTarget(world_.target, w, h, sixteen ? DXGI_FORMAT_B5G6R5_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM) ||
        !makeDepth(w, h, world_.depth, world_.dsv, sixteen)) {
        dropWorld();
        worldScale_ = 1.0;  // no memory for it: the world drawn whole from now on
        if (sixteen) world16Ok_ = false;
        return false;
    }
    world_.sixteen = sixteen;
    LOG_INFO("Direct3D 11: the world drawn at %dx%d (%.0f %% of the picture's %dx%d)%s and made larger, the interface "
             "at the picture's pixels ([quality] world_scale)", w, h, worldScale_ * 100.0, target_.width,
             target_.height, sixteen ? ", 16-bit (super-light)" : "");
    return true;
}

void Renderer11::swapWorld() {
    std::swap(target_, world_.target);
    std::swap(depth_, world_.depth);
    std::swap(dsv_, world_.dsv);
    std::swap(depthSrv_, world_.depthSrv);  // each set keeps its own views (depthViews makes them once)
    std::swap(depthSrvOf_, world_.depthSrvOf);
    std::swap(readOnlyDsv_, world_.readOnlyDsv);
    std::swap(readOnlyDsvOf_, world_.readOnlyDsvOf);
    std::swap(scale_, world_.scale);
    inWorld_ = !inWorld_;
    targetBound_ = false;
}

void Renderer11::enterWorld() {
    if (!ensureWorld()) return;
    flushBatch();
    swapWorld();
}

void Renderer11::exitWorld() {
    if (!inWorld_) return;
    flushBatch();
    glowActive_ = glowDrawn_ = false;  // a glow begun and never drawn into (one drawn was added before this)
    swapWorld();
    const Target& world = world_.target;
    const float all[4] = {0.0f, 0.0f, float(world.width), float(world.height)};
    blit(world.srv, world.width, world.height, all, target_.rtv, Rect{0, 0, target_.width, target_.height},
         target_.width, target_.height, worldSharpen_);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    ctx_->ClearDepthStencilView(dsv_, D3D11_CLEAR_DEPTH, 1.0f, 0);  // the interface's own (its draws mostly keep none)
}

void Renderer11::dropWorld() {
    if (inWorld_) swapWorld();
    dropTarget(world_.target);
    drop(world_.depthSrv);
    drop(world_.readOnlyDsv);
    drop(world_.dsv);
    drop(world_.depth);
    world_.depthSrvOf = world_.readOnlyDsvOf = nullptr;
}

void Renderer11::execute(const FrameRecord& record) {
    if (lost_ || !target_.texture || record.empty()) return;
    LARGE_INTEGER started;  // 0.89: the processor's time in drawing, for the log
    QueryPerformanceCounter(&started);
    timeBegin();
    if (!frameOpen_) {  // 2.5.16: the frame's clear and its world on the world's picture (world_scale)
        frameOpen_ = true;
        if (lightWanted_ != light_) {  // 2.5.20: the light modes' flags, between frames (no world set is in use)
            if ((lightWanted_ ^ light_) & light::kWorld16) dropWorld();
            light_ = lightWanted_;
        }
        enterWorld();
    }
    geometry_ = uploadGeometry(record);
    targetBound_ = false;
    for (const Command& c : record.commands()) {
        if (c.type != Cmd::Draw) flushBatch();  // 0.90: the game's draws before it go first
        switch (c.type) {
            case Cmd::Clear: clear(record.clears()[c.index]); break;
            case Cmd::Viewport:
                gameViewport_ = record.viewports()[c.index];
                if (targetBound_) applyViewport();
                break;
            case Cmd::Upload: upload(record, record.uploads()[c.index]); break;
            case Cmd::WriteTarget: writeTarget(record, record.writes()[c.index]); break;
            case Cmd::Draw: draw(record, record.draws()[c.index]); break;
        }
    }
    flushBatch();
    geometry_ = false;
    timeEnd();
    LARGE_INTEGER ended;
    QueryPerformanceCounter(&ended);
    stats_.ticks += ended.QuadPart - started.QuadPart;
}

bool Renderer11::readTarget(std::vector<uint32_t>& argb) {
    if (lost_ || !target_.texture || !staging_) return false;
    exitWorld();  // 2.5.16: the game reads the whole picture (the rest of the frame drawn on it)
    ID3D11Texture2D* source = target_.texture;
    if (target_.width != gameWidth_ || target_.height != gameHeight_) {
        // The client reads its own size: the target shrunk into the helper first (stage 3 spec §5).
        const float all[4] = {0.0f, 0.0f, float(target_.width), float(target_.height)};
        blit(target_.srv, target_.width, target_.height, all, game_.rtv, Rect{0, 0, gameWidth_, gameHeight_},
             gameWidth_, gameHeight_, -1.0f);
        ctx_->OMSetRenderTargets(0, nullptr, nullptr);
        source = game_.texture;
    }
    ctx_->CopyResource(staging_, source);
    D3D11_MAPPED_SUBRESOURCE m{};
    const HRESULT hr = ctx_->Map(staging_, 0, D3D11_MAP_READ, 0, &m);
    if (FAILED(hr)) {
        failed("reading the frame back", hr);
        return false;
    }
    argb.resize(size_t(gameWidth_) * size_t(gameHeight_));
    for (int y = 0; y < gameHeight_; ++y)
        std::memcpy(argb.data() + size_t(y) * size_t(gameWidth_),
                    static_cast<const uint8_t*>(m.pData) + size_t(y) * m.RowPitch, size_t(gameWidth_) * 4);
    ctx_->Unmap(staging_, 0);
    return true;
}

void Renderer11::releaseSwapChain() {
    drop(backRtv_);
    drop(swap_);
    swapWindow_ = nullptr;
    swapWidth_ = swapHeight_ = 0;
}

bool Renderer11::ensureSwapChain(HWND window, UINT width, UINT height, bool wantTearing) {
    if (swap_ && window == swapWindow_ && width == swapWidth_ && height == swapHeight_) return true;
    if (!swap_ && width == failedWidth_ && height == failedHeight_ && !swapRetry_.mayTry(GetTickCount64()))
        return false;
    targetBound_ = false;
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    drop(backRtv_);
    HRESULT hr = S_OK;
    if (swap_ && window == swapWindow_) {  // the window changed size
        hr = swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, swapFlags_);
        if (FAILED(hr)) {
            failed("swap chain resize", hr);
            releaseSwapChain();
        }
    } else {
        releaseSwapChain();
        ctx_->Flush();
        // Windows 10: the flip model, tearing where it is allowed; 8: the flip model; 7: the old copy model.
        IDXGIFactory2* f2 = nullptr;
        IDXGIFactory5* f5 = nullptr;
        BOOL allow = FALSE;
        if (wantTearing &&
            SUCCEEDED(factory_->QueryInterface(__uuidof(IDXGIFactory5), reinterpret_cast<void**>(&f5))) &&
            FAILED(f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof allow)))
            allow = FALSE;
        drop(f5);
        hr = E_FAIL;
        const char* model = "copy";
        if (SUCCEEDED(factory_->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&f2)))) {
            for (const DXGI_SWAP_EFFECT effect : {DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL}) {
                DXGI_SWAP_CHAIN_DESC1 d{};
                d.Width = width;
                d.Height = height;
                d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                d.SampleDesc.Count = 1;
                d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                // 2.5.14: three - with two, the next frame's buffer is the one on screen until the display takes the
                // new one, and a card slower than the display waited for it each frame: 20 ms of work became 33
                // (R-A327CX, an HD 6570 at 24-31 fps, its "GPU" time the frame's to the tenth of a millisecond)
                d.BufferCount = 3;
                d.SwapEffect = effect;
                d.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
                d.Flags = effect == DXGI_SWAP_EFFECT_FLIP_DISCARD && allow ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
                IDXGISwapChain1* sc1 = nullptr;
                hr = f2->CreateSwapChainForHwnd(device_, window, &d, nullptr, nullptr, &sc1);
                if (SUCCEEDED(hr)) {
                    swap_ = sc1;
                    swapFlags_ = d.Flags;
                    tearing_ = d.Flags != 0;
                    model = effect == DXGI_SWAP_EFFECT_FLIP_DISCARD ? "flip" : "flip (sequential)";
                    break;
                }
            }
            drop(f2);
        }
        if (!swap_) {
            DXGI_SWAP_CHAIN_DESC d{};
            d.BufferDesc.Width = width;
            d.BufferDesc.Height = height;
            d.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            d.SampleDesc.Count = 1;
            d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            d.BufferCount = 1;
            d.OutputWindow = window;
            d.Windowed = TRUE;
            d.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            hr = factory_->CreateSwapChain(device_, &d, &swap_);
            swapFlags_ = 0;
            tearing_ = false;
        }
        if (FAILED(hr) || !swap_) {
            failed("swap chain creation", hr);
            drop(swap_);
        } else {
            factory_->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
            swapWindow_ = window;
            LOG_INFO("swap chain %ux%u (Direct3D 11, %s model%s)", width, height, model,
                     tearing_ ? ", tearing allowed" : "");
        }
    }
    ID3D11Texture2D* back = nullptr;
    if (swap_) hr = swap_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back));
    if (swap_ && SUCCEEDED(hr)) hr = device_->CreateRenderTargetView(back, nullptr, &backRtv_);
    drop(back);
    if (!swap_ || FAILED(hr)) {
        if (swap_) failed("the swap chain's picture", hr);
        releaseSwapChain();
        failedWidth_ = width;
        failedHeight_ = height;
        swapRetry_.failed(GetTickCount64());
        return false;
    }
    swapWidth_ = width;
    swapHeight_ = height;
    failedWidth_ = failedHeight_ = 0;
    swapRetry_.succeeded();
    return true;
}

void Renderer11::present(HWND window, int outputWidth, int outputHeight, const Rect& source,
                         const PresentOptions& options) {
    LARGE_INTEGER started;  // 0.89: the processor's time in drawing (not the waits for the frame's turn)
    QueryPerformanceCounter(&started);
    if (lost_) {  // no Present waits for vblank: keep the game's loop from spinning
        Sleep(15);
        return;
    }
    if (ai_) ai_->work();  // 0.47: the AI textures' pictures read meanwhile go in
    collectVariants();     // 2.5.16: the draw shaders made meanwhile
    mapWorld_.poll();      // 2.5.1: the map's file read meanwhile
    if (!target_.texture || !window || source.empty()) return;
    if (!ensureSwapChain(window, UINT(std::max(1, outputWidth)), UINT(std::max(1, outputHeight)),
                         options.allowTearing)) {
        Sleep(15);
        return;
    }
    timeBegin();
    effectEnd(kTimeWorld11);  // 2.5.15: a frame without an interface ends its world here
    effectEnd(kTimeUi11);
    // Fit the source into the window, keeping its shape, on whole pixels: bars where the shapes differ.
    const Rect fit = fitPicture(source.width(), source.height(), int(swapWidth_), int(swapHeight_));
    if (!(fit == picture_)) {
        picture_ = fit;
        LOG_DEBUG("picture %dx%d of the game shown as %dx%d at %d,%d in the %ux%u window", source.width(),
                  source.height(), fit.width(), fit.height(), fit.left, fit.top, swapWidth_, swapHeight_);
    }
    if (worldDrawn_ && !stepsDone_) applySteps();  // 2.5.2
    if (worldDrawn_ && !nightDone_) applyNight();  // 0.63
    if (worldDrawn_ && !lightDone_) {
        applyTorchLight();  // 0.61
        applyEmbers();      // 2.5.3
        applyCritters();    // 2.5.4
    }
    if (worldDrawn_ && !floorDone_) applyFloorReflections();  // 2.5.3
    if (worldDrawn_ && !mistDone_) applyMist();  // 2.5
    if (worldDrawn_ && !skyDone_) applySky();
    if (worldDrawn_ && !shaftsDone_) applyShafts();  // 2.5.5
    if (worldDrawn_ && !flareDone_) applyFlare();
    if (worldDrawn_ && !heatDone_) applyHeat();  // 2.5.3
    if (!flies_.empty()) applyFireflies();  // 1.0.1
    if (worldDrawn_ && !bloomDone_) applyBloom();  // 2.5.2
    if (worldDrawn_ && !gradeDone_) applyGrade();  // 0.59: a frame without an interface
    if (worldDrawn_ && !weatherDone_) applyWeather();  // 2.5
    if (!darknessLaid_) applyDarkness();  // 2.6.18
    if (glowDrawn_) addGlow();  // 1.0: likewise
    exitWorld();  // 2.5.16: a frame without an interface: its world made larger now
    frameOpen_ = false;
    scaled_ = true;  // the start-up calibration is over by the first present (stage 3 spec §4.3)
    // the world made larger gets a little sharpening of its own unless the present sharpens all of it - or the light
    // modes want it plain (2.5.22: Catmull-Rom and CAS, 45 taps a pixel, cost more than the world they made larger)
    worldSharpen_ = options.sharpen > 0.0f || (light_ & light::kPlainUpscale) ? -1.0f : 0.3f;
    // The source is in game pixels: into target pixels by each axis' own ratio (2.5.16: the picture's, the world's
    // smaller one made larger into it by now).
    const auto tx = [this](int x) { return float(double(x) * target_.width / gameWidth_); };
    const auto ty = [this](int y) { return float(double(y) * target_.height / gameHeight_); };
    const float from[4] = {tx(source.left), ty(source.top), tx(source.right), ty(source.bottom)};
    // 2.5.14: the window's buffer last, outside the card's timed work - its first use waits until the display lets
    // go of it, and that wait was counted as the card's (keep_fps turned effects off for it)
    timeEnd();
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ctx_->ClearRenderTargetView(backRtv_, black);
    timeBegin();
    blit(target_.srv, target_.width, target_.height, from, backRtv_, fit, int(swapWidth_), int(swapHeight_),
         options.sharpen);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    if (!options.screenshot.empty()) saveScreenshot(options.screenshot);  // 0.54
    gradeDone_ = worldDrawn_ = false;  // 0.59: the next frame
    groundDrawn_ = shadowsDone_ = false;  // 0.60.1
    aoDone_ = false;  // 2.5.1
    fireSparks_.clear();
    lightDone_ = nightDone_ = false;  // 0.61, 0.63
    mistDone_ = skyDone_ = weatherDone_ = false;  // 2.5
    bloomDone_ = stepsDone_ = false;  // 2.5.2
    heatDone_ = floorDone_ = false;  // 2.5.3
    heatSpots_.clear();
    embers_.clear();
    groundLifeDone_ = grassDone_ = waterLevelNoted_ = false;  // 2.5.4
    frameCrowns_.clear();
    critterFires_.clear();
    critterFlames_.clear();  // 2.6.12
    darknessRec_.reset();  // 2.6.18
    darknessLaid_ = false;
    shaftsDone_ = flareDone_ = false;  // 2.5.5
    shaftLights_ = 0;
    updateWind();
    if (grassWanted() && !groundTris_.empty()) groundCells_ = ground_cells::build(groundTris_);  // the next frame's
    if (hillsWanted()) hillsGrid_ = hills::build(groundTris_);  // 2.5.9: the next frame's smooth hills
    groundTris_.clear();
    groundGroups_.clear();  // 2.5.9: the seams' groups
    groundTextures_.clear();
    groundCellsUp_ = false;
    hillsUp_ = hillsTried_ = seamsDone_ = false;
    if (mirrorCollecting()) {  // 2.5.9: this frame's opaque world for the next one's mirror
        mirrorVerts_.swap(casterVerts_);
        mirrorCasters_.swap(casters_);
    }
    casterVerts_.clear();
    casters_.clear();
    mirrorDone_ = mirrorMade_ = false;
    if (!frameFeet_.empty()) {  // 2.5.2: this frame's feet (as Direct3D 12's newPicture)
        footTracker_.update(frameFeet_, waterTime(), stepLife());
        frameFeet_.clear();
    }
    waterCopied_ = waterReady_ = false;  // 0.99
    materialsMade_ = 0;  // 1.14
    flames_.clear();
    timeEnd();
    LARGE_INTEGER ended;  // 0.89
    QueryPerformanceCounter(&ended);
    stats_.ticks += ended.QuadPart - started.QuadPart;
    ++stats_.frames;
    timeFrame();
    pacer_.wait(options.fpsLimit, options.pacerSleeps);  // 0.85: kept to the frame
    const UINT sync = options.vsync ? 1 : 0;
    const UINT flags = !options.vsync && options.allowTearing && tearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0;
    const HRESULT hr = swap_->Present(sync, flags);
    if (FAILED(hr))
        failed("Present", hr);
    else if (hr == DXGI_STATUS_OCCLUDED)
        Sleep(15);  // the window is covered: Present does not wait for vblank
}

// 0.59: the Effects page's colours on Direct3D 11 - grade_ps from a copy of the picture into it, RGB only (as
// Direct3D 12's Impl::applyGrade).
void Renderer11::applyNight() {  // 0.63: as the colours, night_ps by its strength now
    EffectSpan span(*this, kTimeNight11);  // 0.89
    nightDone_ = true;
    const bool night = effects_.nightNow > 0, day = effects_.dayCycle && effects_.dayNow >= 0;
    const region_look::Look look = region_look::of(map_, effects_.regionLooks ? effects_.regionLooksStrength : 0);
    // 2.5.21: the region's look only when it changes the picture - with region_looks off this was a pass for nothing
    // on every map of a region, and with super-light's 16-bit world (no copy of it into scene_) a black world
    if ((!night && !day && !region_look::changes(look)) || !nightPs_ || !target_.texture) return;
    Constants c{};
    c.sharpen = night ? float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f : 0.0f;
    const daylight::Light light = daylight::at(day ? effects_.dayNow : -1);  // 0.74: the time of day's tint
    for (int k = 0; k < 3; ++k) c.twin[k] = light.tint[k] * look.tint[k];  // 0.79: and the region's look
    c.sourceRect[3] = look.saturation - 1.0f;
    c.uiPixels[0] = float(target_.width);
    c.uiPixels[1] = float(target_.height);
    worldPass(nightPs_, c);
}

void Renderer11::applyGrade() {
    EffectSpan span(*this, kTimeGrade11);  // 0.89
    gradeDone_ = true;
    if (!effects_.grade || !gradePs_ || !target_.texture) return;
    Constants c{};
    c.sharpen = float(std::clamp(effects_.gradeSaturation, 0, 200)) / 100.0f;
    c.footprint = float(std::clamp(effects_.gradeContrast, 50, 150)) / 100.0f;
    c.twin[0] = float(std::clamp(effects_.gradeBrightness, 50, 150)) / 100.0f;
    c.twin[1] = float(std::clamp(effects_.gradeWarmth, 0, 100) - 50) / 50.0f;
    c.twin[2] = float(std::clamp(effects_.gradeVignette, 0, 100)) / 100.0f;
    c.uiPixels[0] = float(target_.width);
    c.uiPixels[1] = float(target_.height);
    worldPass(gradePs_, c);
}

// A pass over the world from a copy of it (scene_, t2) into it, RGB only: the colours (0.59), the night (0.63).
void Renderer11::worldPass(ID3D11PixelShader* ps, const Constants& c, int copy, int blend,
                           ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* t0,
                           ID3D11ShaderResourceView* t1, const RECT* clip, ID3D11ShaderResourceView* t5) {
    if (copy == 1 && !copyScene()) return;
    targetBound_ = false;
    ID3D11ShaderResourceView* none[4] = {nullptr, nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 4, none);
    ctx_->OMSetRenderTargets(1, &target_.rtv, nullptr);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(target_.width), float(target_.height), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    const D3D11_RECT all{0, 0, target_.width, target_.height};
    ctx_->RSSetScissorRects(1, clip ? clip : &all);  // 2.5.3: only these pixels when given
    setConstants(c);
    // 1: over by its alpha; 2: the picture times it (blend ZERO, SRCCOLOR - the shadows); 3: added; 4: premultiplied
    // over (ONE, INVSRCALPHA); 0: written whole
    const uint32_t src = blend == 1 ? d3d7::BLEND_SRCALPHA : blend == 2 ? d3d7::BLEND_ZERO : d3d7::BLEND_ONE;
    const uint32_t dst = blend == 1 || blend == 4 ? d3d7::BLEND_INVSRCALPHA
                         : blend == 2             ? d3d7::BLEND_SRCCOLOR
                         : blend == 3             ? d3d7::BLEND_ONE
                                                  : d3d7::BLEND_ZERO;
    ctx_->OMSetBlendState(blendState(blend != 0, src, dst,
                                     D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                                         D3D11_COLOR_WRITE_ENABLE_BLUE),
                          nullptr, 0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->PSSetShader(ps, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    passSamplers();
    if (t0) ctx_->PSSetShaderResources(0, 1, &t0);            // tex0 : register(t0) - a mask of its own
    if (t1) ctx_->PSSetShaderResources(1, 1, &t1);            // tex1 : register(t1) - a list of its own
    if (t5) ctx_->PSSetShaderResources(5, 1, &t5);            // 2.5.4: register(t5) - the ground's cells
    if (copy != 0) ctx_->PSSetShaderResources(2, 1, &scene_.srv);  // sceneColor : register(t2)
    if (depth) ctx_->PSSetShaderResources(3, 1, &depth);     // sceneDepth : register(t3)
    ctx_->Draw(3, 0);
    ctx_->PSSetShaderResources(0, 4, none);
    if (t5) ctx_->PSSetShaderResources(5, 1, none);
    if (clip) ctx_->RSSetScissorRects(1, &all);
}

void Renderer11::passSamplers() {
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    ID3D11SamplerState* samplers[2] = {samplerState(linearClamp, 0), samplerState(linearClamp, 0)};
    ctx_->PSSetSamplers(0, 2, samplers);
}

void Renderer11::maskPass(ID3D11PixelShader* ps, const Target& into, const Constants& c,
                          ID3D11ShaderResourceView* t0, ID3D11ShaderResourceView* depth,
                          ID3D11ShaderResourceView* t2, ID3D11ShaderResourceView* t1) {
    targetBound_ = false;  // the next draw binds the target, the viewport and the game's shaders again
    ID3D11ShaderResourceView* none[4] = {nullptr, nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 4, none);
    ctx_->OMSetRenderTargets(1, &into.rtv, nullptr);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(into.width), float(into.height), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    const D3D11_RECT all{0, 0, into.width, into.height};
    ctx_->RSSetScissorRects(1, &all);
    setConstants(c);
    // 2.5.7: all four channels, as Direct3D 12's (fullScreenPipeline): the alpha was never written - half_over_ps
    // reads it (none: nothing laid over), so the floors' reflections (2.5.3) could not show on Direct3D 11
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO, D3D11_COLOR_WRITE_ENABLE_ALL), nullptr,
                          0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->PSSetShader(ps, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    passSamplers();
    ID3D11ShaderResourceView* views[4] = {t0, t1, t2, depth};
    ctx_->PSSetShaderResources(0, 4, views);
    ctx_->Draw(3, 0);
    ctx_->PSSetShaderResources(0, 4, none);
}

// 2.5.2 (actor_shadows.cpp noteFoot): a character's round shadow - its middle in the world, the feet this frame
void Renderer11::noteFoot(const FrameRecord& rec, const DrawCmd& d) {
    if (!cameraValid_ || d.vertexCount != 4 || frameFeet_.size() >= 256) return;
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > rec.vertexBytes().size()) return;
    const uint8_t* q = rec.vertexBytes().data() + d.vertexOffset;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, rhw = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, q + size_t(i) * stride, sizeof xyzw);
        x0 = std::min(x0, xyzw[0]);
        x1 = std::max(x1, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3] * 0.25f;
    }
    if (rhw <= 0.0f) return;
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float* m = viewToWorld_;
    const float vz = 1.0f / rhw, vx = ((x0 + x1) * 0.5f - proj.cx) * vz / proj.f0,
                vy = ((y0 + y1) * 0.5f - proj.cy) * vz / proj.f1;
    frameFeet_.push_back(map::Point{vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                                    vx * m[2] + vy * m[5] + vz * m[8] + m[11]});
}

// 2.5.2 (torches.cpp noteLava): a cell of lava - its middle in the view, for the light it gives
void Renderer11::noteLava(const FrameRecord& rec, const DrawCmd& d) {
    if (lavaCells_.size() >= 4096) return;
    const size_t stride = FrameRecord::stride(d.fvf);
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    FireSpark11 s{};
    float rhw = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, rec.vertexBytes().data() + d.vertexOffset + size_t(i) * stride, sizeof xyzw);
        s.gameX += xyzw[0] * 0.25f;
        s.gameY += xyzw[1] * 0.25f;
        rhw += xyzw[3] * 0.25f;
    }
    if (rhw <= 0.0f) return;
    const float vz = 1.0f / rhw;
    s.view[0] = (s.gameX - p.cx) * vz / p.f0;
    s.view[1] = (s.gameY - p.cy) * vz / p.f1;
    s.view[2] = vz;
    s.strength = 1.0f;
    lavaCells_.push_back(s);
}

// 2.5.2: as Direct3D 12's applySteps (ao.cpp) - the steps' list (steps_ps's t1: two texels a step, its place and
// way, then its age) over the ground, blended by its alpha.
void Renderer11::applySteps() {
    EffectSpan span(*this, kTimeSteps11);
    stepsDone_ = true;
    const int kind = stepKind();
    const std::deque<footsteps::Step>& steps = footTracker_.steps();
    if (!kind || steps.empty() || !cameraValid_ || !target_.texture || !depthViews()) return;
    if (!stepsData_) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = UINT(footsteps::Tracker::kMax * 2);
        d.Height = 1;
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &stepsData_)) ||
            FAILED(device_->CreateShaderResourceView(stepsData_, nullptr, &stepsDataSrv_))) {
            drop(stepsData_);
            LOG_ERROR("footprints: no memory for their list (Direct3D 11): none");
            drop(stepsPs_);  // not tried again
            return;
        }
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
    ctx_->UpdateSubresource(stepsData_, 0, nullptr, data, UINT(sizeof data), 0);
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);
    c.scale = float(scale_);
    c.uiPixels[0] = float(target_.width);
    c.uiPixels[1] = float(target_.height);
    c.footprint = float(n);
    c.twin[0] = float(kind);
    if (!stepsLogged_)
        LOG_INFO("footprints: %s on %s (Direct3D 11)", kind == 1 ? "in the snow" : kind == 2 ? "in the sand" : "dust",
                 map_.c_str());
    stepsLogged_ = true;
    worldPass(stepsPs_, c, 0, 1, depthSrv_, nullptr, stepsDataSrv_);
}

// 2.5.3: as Direct3D 12's applyHeat (heat.cpp) - the spots of heat the torches' pass gathered (a texel each in
// heat_ps's t1) and the desert far off: the picture read from its copy a little aside, cut to where they are.
void Renderer11::applyHeat() {
    EffectSpan span(*this, kTimeHeat11);
    heatDone_ = true;
    std::vector<HeatSpot11> spots;
    spots.swap(heatSpots_);
    const float desertAmount =
        heatWanted() && effects_.desertHeat && desert_ && cameraValid_ ? 0.35f * heatUser() : 0.0f;
    if ((spots.empty() && desertAmount <= 0.0f) || !heatPs_ || !target_.texture || !depthViews()) return;
    constexpr size_t kMaxHeat = 32;
    if (!heatData_) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = UINT(kMaxHeat);
        d.Height = 1;
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &heatData_)) ||
            FAILED(device_->CreateShaderResourceView(heatData_, nullptr, &heatDataSrv_))) {
            drop(heatData_);
            LOG_ERROR("heat haze: no memory for its list (Direct3D 11): none");
            drop(heatPs_);  // not tried again
            return;
        }
    }
    const int w = target_.width, h = target_.height;
    std::sort(spots.begin(), spots.end(), [](const HeatSpot11& a, const HeatSpot11& b) {
        return std::fabs(a.strength) > std::fabs(b.strength);
    });
    if (spots.size() > kMaxHeat) spots.resize(kMaxHeat);
    float data[kMaxHeat][4] = {};
    float x0 = float(w), y0 = float(h), x1 = 0.0f, y1 = 0.0f;
    for (size_t i = 0; i < spots.size(); ++i) {
        const HeatSpot11& s = spots[i];
        data[i][0] = s.x;
        data[i][1] = s.y;
        data[i][2] = s.radius;
        data[i][3] = std::clamp(s.strength, -1.5f, 1.5f);  // the player's setting is in it; below 0 a swirl
        x0 = std::min(x0, s.x - s.radius * 1.2f);
        x1 = std::max(x1, s.x + s.radius * 1.2f);
        y0 = std::min(y0, s.y - s.radius * 2.0f);  // the hot air rises
        y1 = std::max(y1, s.y + s.radius * 0.6f);
    }
    D3D11_RECT clip{0, 0, w, h};
    if (desertAmount <= 0.0f)
        clip = D3D11_RECT{LONG(std::max(0.0f, std::floor(x0))), LONG(std::max(0.0f, std::floor(y0))),
                          LONG(std::min(float(w), std::ceil(x1))), LONG(std::min(float(h), std::ceil(y1)))};
    if (clip.right <= clip.left || clip.bottom <= clip.top) return;
    ctx_->UpdateSubresource(heatData_, 0, nullptr, data, UINT(sizeof data), 0);
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(scale_);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = float(spots.size());
    c.twin[0] = desertAmount;
    c.sharpen = waterTime();
    if (!heatLogged_)
        LOG_INFO("heat haze: %u spots of heat%s (Direct3D 11)", unsigned(spots.size()), desert_ ? ", the desert" : "");
    heatLogged_ = true;
    worldPass(heatPs_, c, 1, 0, depthSrv_, nullptr, heatDataSrv_, &clip);
}

// 2.5.3 (torches.cpp notePortal): a portal's draw - its middle in the view, its size on the screen
void Renderer11::notePortal(const FrameRecord& rec, const DrawCmd& d) {
    if (portalSparks_.size() >= 256) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const uint32_t n = std::min<uint32_t>(d.vertexCount, 64);
    if (n == 0 || d.vertexOffset + size_t(n) * stride > v.size()) return;
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, rhw = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + size_t(i) * stride, sizeof xyzw);
        x0 = std::min(x0, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        x1 = std::max(x1, xyzw[0]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3];
    }
    rhw /= float(n);
    if (rhw <= 0.0f) return;
    FireSpark11 s{};
    s.gameX = (x0 + x1) * 0.5f;
    s.gameY = (y0 + y1) * 0.5f;
    const float vz = 1.0f / rhw;
    s.view[0] = (s.gameX - p.cx) * vz / p.f0;
    s.view[1] = (s.gameY - p.cy) * vz / p.f1;
    s.view[2] = vz;
    s.strength = std::max(x1 - x0, y1 - y0);  // its size on the screen, game pixels
    portalSparks_.push_back(s);
}

// 2.5.3 (torches.cpp noteRay): a face of one of the forest's beams - its middle, and its foot (the end its texture's
// v reaches 1 at, the ground) - in the view
void Renderer11::noteRay(const FrameRecord& rec, const DrawCmd& d) {
    if (rayParts_.size() >= 512) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const uint32_t n = std::min<uint32_t>(d.vertexCount, 64);
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    RayPart11 r{};
    float sx = 0.0f, sy = 0.0f;
    int all = 0, feet = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* q = v.data() + d.vertexOffset + size_t(i) * stride;
        float xyzw[4], uv[2];
        std::memcpy(xyzw, q, sizeof xyzw);
        std::memcpy(uv, q + 24, sizeof uv);
        if (xyzw[3] <= 0.0f) continue;
        const float vz = 1.0f / xyzw[3];
        const float view[3] = {(xyzw[0] - p.cx) * vz / p.f0, (xyzw[1] - p.cy) * vz / p.f1, vz};
        for (int k = 0; k < 3; ++k) r.middle[k] += view[k];
        sx += xyzw[0];
        sy += xyzw[1];
        ++all;
        if (uv[1] >= 0.5f) {
            for (int k = 0; k < 3; ++k) r.foot[k] += view[k];
            ++feet;
        }
    }
    if (all == 0 || feet == 0) return;
    for (int k = 0; k < 3; ++k) {
        r.middle[k] /= float(all);
        r.foot[k] /= float(feet);
    }
    r.gameX = sx / float(all);
    r.gameY = sy / float(all);
    rayParts_.push_back(r);
}

// 2.5.3: as Direct3D 12's applyEmbers (torches.cpp) - the places the torches' pass gathered (two texels each in
// embers_ps's t1), the sparks added on, cut to them.
void Renderer11::applyEmbers() {
    std::vector<Ember11> places;
    places.swap(embers_);
    if (places.empty() || !embersPs_ || !target_.texture) return;
    constexpr size_t kMaxEmbers = 24;
    if (!embersData_) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = UINT(kMaxEmbers * 2);
        d.Height = 1;
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &embersData_)) ||
            FAILED(device_->CreateShaderResourceView(embersData_, nullptr, &embersDataSrv_))) {
            drop(embersData_);
            drop(embersPs_);  // not tried again
            return;
        }
    }
    float data[kMaxEmbers * 2][4] = {};
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    const size_t n = std::min(places.size(), kMaxEmbers);
    for (size_t i = 0; i < n; ++i) {
        const Ember11& e = places[i];
        data[2 * i][0] = e.x;
        data[2 * i][1] = e.y;
        data[2 * i][2] = e.size;
        data[2 * i][3] = e.strength;
        std::copy(e.colour, e.colour + 3, data[2 * i + 1]);
        data[2 * i + 1][3] = e.fire ? 1.0f : 0.0f;
        x0 = std::min(x0, e.x - 3.0f * e.size);
        y0 = std::min(y0, e.y - 3.0f * e.size);
        x1 = std::max(x1, e.x + 3.0f * e.size);
        y1 = std::max(y1, e.y + 3.0f * e.size);
    }
    const int w = target_.width, h = target_.height;
    const D3D11_RECT lit{LONG(std::max(0.0f, std::floor(x0))), LONG(std::max(0.0f, std::floor(y0))),
                         LONG(std::min(float(w), std::ceil(x1))), LONG(std::min(float(h), std::ceil(y1)))};
    if (lit.right <= lit.left || lit.bottom <= lit.top) return;
    ctx_->UpdateSubresource(embersData_, 0, nullptr, data, UINT(sizeof data), 0);
    Constants c{};
    c.scale = float(scale_);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = float(n);
    c.sharpen = waterTime();
    worldPass(embersPs_, c, 0, 3, nullptr, nullptr, embersDataSrv_, &lit);
}

// 2.5.3: as Direct3D 12's applyFloorReflections (haze.cpp) - the floors reflect what stands on them, indoors and in
// the rain: worked out at half the size from the picture's copy and its depth (floor_reflect_ps), laid over smoothly
// (half_over_ps, premultiplied).
void Renderer11::applyFloorReflections() {
    EffectSpan span(*this, kTimeFloor11);  // 2.5.9
    floorDone_ = true;
    const float amount = floorReflectAmount();
    if ((amount <= 0.0f && !wetWanted()) || !cameraValid_ || !target_.texture || !depthViews()) return;
    const int w = target_.width, h = target_.height;
    const int hw = std::max(1, (w + 1) / 2), hh = std::max(1, (h + 1) / 2);
    if (floor_.width != hw || floor_.height != hh) {
        dropTarget(floor_);
        if (!makeTarget(floor_, hw, hh)) {
            LOG_ERROR("floor reflections: no video memory at %dx%d (Direct3D 11): none", hw, hh);
            halfOverPs_ = nullptr;  // not tried again (the object stays until the end)
            return;
        }
    }
    if (!copyScene()) return;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);
    c.scale = float(scale_);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.twin[0] = amount;
    c.twin[1] = wetWanted() ? 1.0f : 0.0f;
    c.sharpen = waterTime();  // the drops' rings in the puddles
    if (!floorLogged_) LOG_INFO("floor reflections: on %s, at %dx%d (Direct3D 11)", map_.c_str(), hw, hh);
    floorLogged_ = true;
    maskPass(floorReflectPs_, floor_, c, nullptr, depthSrv_, scene_.srv);
    worldPass(halfOverPs_, c, 0, 4, nullptr, floor_.srv);
}

// 2.5.4 (draw.cpp noteWaterLevel): the water's height in the world, from a cell of it - once a frame
void Renderer11::noteWaterLevel(const FrameRecord& rec, const DrawCmd& d) {
    if (!cameraValid_ || waterLevelNoted_) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    if (d.vertexOffset + 16 > v.size()) return;
    float xyzw[4];
    std::memcpy(xyzw, v.data() + d.vertexOffset, sizeof xyzw);
    if (xyzw[3] <= 0.0f) return;
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const float* m = viewToWorld_;
    const float vz = 1.0f / xyzw[3], vx = (xyzw[0] - p.cx) * vz / p.f0, vy = (xyzw[1] - p.cy) * vz / p.f1;
    waterLevel_ = vx * m[1] + vy * m[4] + vz * m[7] + m[10];
    waterLevelNoted_ = waterLevelKnown_ = true;
}

// 2.5.4 (ground.cpp noteGround): a draw of the ground - its triangles in the world, for the grass's cells
// 2.5.4: as Direct3D 12's applyGroundLife (ao.cpp) - ground_life_ps from the picture's copy and its depth, blended
void Renderer11::applyGroundLife() {
    groundLifeDone_ = true;
    if (!groundLifeWanted() || !cameraValid_ || !target_.texture || !depthViews()) return;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);
    c.scale = float(scale_);
    c.uiPixels[0] = float(target_.width);
    c.uiPixels[1] = float(target_.height);
    const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    c.twin[0] = effects_.sandDrift && place_.sand ? 1.0f : 0.0f;
    c.twin[1] = effects_.snowSparkle && place_.snow ? 1.0f - 0.6f * night : 0.0f;
    c.twin[2] = effects_.wetWalls && place_.cave ? 1.0f : 0.0f;
    if (wetSandWanted()) {  // the wet sand along the shore
        c.twin[3] = 1.0f;
        c.sourceClamp[0] = waterLevel_;
    }
    if (seasonsShown()) {  // the season's snow and grass, each a box of its own
        c.sourceRect[0] = effects_.seasonSnow ? year_.snow : 0.0f;
        c.sourceRect[2] = effects_.seasonGrass ? year_.dry : 0.0f;
        c.sourceRect[3] = effects_.seasonGrass ? year_.lush : 0.0f;
    }
    if (rainWetShown()) c.sourceClamp[2] = 1.0f;
    if (windShared()) {  // 2.5.5: the sand drifts the shared wind's way
        c.uiPixels[2] = windNow_.x;
        c.uiPixels[3] = windNow_.z;
    }
    c.sharpen = waterTime();
    if (!groundLifeLogged_)
        LOG_INFO("living ground: on %s (%s%s%s, Direct3D 11)", map_.c_str(),
                 c.twin[0] > 0.0f   ? "sand"
                 : c.twin[1] > 0.0f ? "snow"
                 : c.twin[2] > 0.0f ? "damp walls"
                 : c.twin[3] > 0.0f ? "wet sand"
                                    : "nothing of its own",
                 seasonsShown() ? ", the season" : "", rainWetShown() ? ", wet in the rain" : "");
    groundLifeLogged_ = true;
    worldPass(groundLifePs_, c, 1, 1, depthSrv_);
}

// 2.6.8: as Direct3D 12's uploadTrample (ao.cpp) - the steps' hold on the blades made into its grid and texture
bool Renderer11::uploadTrample() {
    if (trampleFailed_) return false;
    trample::build(footTracker_.steps(), waterTime(), stepLife(), trample_);
    if (trample_.texels.empty()) return false;
    constexpr UINT side = UINT(trample::Grid::kSide);
    if (!trampleData_) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = d.Height = side;
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &trampleData_)) ||
            FAILED(device_->CreateShaderResourceView(trampleData_, nullptr, &trampleSrv_))) {
            drop(trampleData_);
            trampleFailed_ = true;  // not tried again
            LOG_ERROR("grass: no memory for the steps' grid (Direct3D 11): the blades stand where steps were");
            return false;
        }
    }
    ctx_->UpdateSubresource(trampleData_, 0, nullptr, trample_.texels.data(), side * 4, 0);
    return true;
}

// 2.5.4: as Direct3D 12's applyGrass (ao.cpp) - grass_ps from the picture's copy and its depth, blended over; the
// steps bend it (t1: 2.6.8 their hold's grid, uploadTrample), the map's ground only (t5, its cells' heights from the
// last frame).
void Renderer11::applyGrass() {
    EffectSpan span(*this, kTimeGrass11);  // 2.5.9
    grassDone_ = true;
    if (!grassWanted() || !target_.texture || !depthViews()) return;
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);
    const float k = float(scale_);
    c.scale = k;
    c.uiPixels[0] = float(target_.width);
    c.uiPixels[1] = float(target_.height);
    const bool held = uploadTrample();  // 2.6.8: the steps' hold read from its grid, not each step at every pixel
    if (held) {
        c.footprint = 1.0f;
        c.fogColor[0] = trample_.originX;
        c.fogColor[1] = trample_.originZ;
        c.fogColor[2] = 1.0f / trample_.cell;
        c.fogColor[3] = float(trample::Grid::kSide);
    }
    // 2.6.8: Direct3D 11 is a weak card's (an HD 6570's grass cost 22 ms, R-9K2F6E): the roots on every second row
    // only - each blade still found, the search half as long
    c.uiPixels[2] = 2.0f;
    c.uiPixels[3] = waterLevelKnown_ ? waterLevel_ + 0.05f : 1e9f;  // 2.7.2: no blades under the water (grass_ps)
    c.twin[0] = std::clamp(std::round(1.6f * proj.f0 * k / 350.0f), 4.0f, 16.0f);  // Classic+'s longest blade
    c.twin[1] = 0.65f * float(std::clamp(effects_.grassAmount, 0, 100)) / 100.0f;
    c.sharpen = waterTime();
    if (effects_.flowers && place_.flowers >= 0) {  // flowers among the blades, in the patches only
        c.twin[2] = 0.12f;
        c.sourceRect[1] = float(place_.flowers);
    }
    const int minute = effects_.dayNow >= 0 ? effects_.dayNow : living::localMinute();  // 2.6.11
    if (effects_.dew) c.twin[3] = living::dew(minute) * (1.0f - float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f);
    if (effects_.gusts) c.sourceRect[0] = 1.0f;
    if (windShared()) {  // 2.5.5: the shared wind's way times its strength
        c.sourceRect[2] = windNow_.x * windNow_.strength;
        c.sourceRect[3] = windNow_.z * windNow_.strength;
    }
    // The map's ground only: the cells' grid (sourceClamp), their heights (t5) - uploaded once a frame
    const ground_cells::Grid& g = groundCells_;
    const bool grid = g.side > 0.0f && g.width > 0 && g.height > 0 &&
                      g.span.size() >= size_t(g.width) * size_t(g.height) * 2;
    if (grid && !groundCellsData_) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = d.Height = UINT(ground_cells::kMostSide);
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R32G32_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &groundCellsData_)) ||
            FAILED(device_->CreateShaderResourceView(groundCellsData_, nullptr, &groundCellsSrv_))) {
            drop(groundCellsData_);
            LOG_ERROR("grass: no memory for the ground's heights (Direct3D 11): on every green pixel");
        }
    }
    if (grid && groundCellsSrv_ && !groundCellsUp_) {
        const D3D11_BOX box{0, 0, 0, UINT(g.width), UINT(g.height), 1};
        ctx_->UpdateSubresource(groundCellsData_, 0, &box, g.span.data(), UINT(g.width) * 2 * sizeof(float), 0);
        groundCellsUp_ = true;
    }
    if (grid && groundCellsUp_) {
        c.sourceClamp[0] = g.originX;
        c.sourceClamp[1] = g.originZ;
        c.sourceClamp[2] = 1.0f / g.side;
        c.sourceClamp[3] = float(g.width) + 1024.0f * float(g.height);
    }
    if (!grassLogged_)
        LOG_INFO("grass: on %s, blades up to %.0f pixels%s, their roots on every second row (Direct3D 11)",
                 map_.c_str(), c.twin[0], grid && groundCellsUp_ ? ", on the map's ground only" : "");
    grassLogged_ = true;
    worldPass(grassPs_, c, 1, 1, depthSrv_, nullptr, held ? trampleSrv_ : nullptr, nullptr,
              grid && groundCellsUp_ ? groundCellsSrv_ : nullptr);
}

// 2.5.4 (critters.cpp noteCrown): a tree's leaves drawn - its crown in the world, for the leaves falling from it
void Renderer11::noteCrown(const FrameRecord& rec, const DrawCmd& d) {
    if (!cameraValid_ || frameCrowns_.size() >= 32) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, rhw = 0.0f;
    uint32_t n = 0;
    for (uint32_t i = 0; i < d.vertexCount && d.vertexOffset + (i + 1) * stride <= v.size(); ++i, ++n) {
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + i * stride, sizeof xyzw);
        x0 = std::min(x0, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        x1 = std::max(x1, xyzw[0]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3];
    }
    if (!n || rhw <= 0.0f || x1 - x0 < 8.0f) return;
    rhw /= float(n);
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const float* m = viewToWorld_;
    const float vz = 1.0f / rhw, vx = ((x0 + x1) * 0.5f - p.cx) * vz / p.f0, vy = ((y0 + y1) * 0.5f - p.cy) * vz / p.f1;
    critters::Crown crown;
    crown.centre = critters::Point{vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                                   vx * m[2] + vy * m[5] + vz * m[8] + m[11]};
    crown.radius = std::clamp((x1 - x0) * 0.5f * vz / p.f0, 1.0f, 12.0f);
    frameCrowns_.push_back(crown);
}

// 2.5.4: as Direct3D 12's applyCritters (critters.cpp) - the creatures moved on about the player (the point the
// camera looks at), turned to the screen into four texels each (t1) and drawn as quads by critters_vs, premultiplied
// over the picture, hidden by its depth (t3).
void Renderer11::applyCritters() {
    std::vector<critters::Crown> crowns;
    crowns.swap(frameCrowns_);
    std::vector<critters::Point> fires;
    fires.swap(critterFires_);
    std::vector<torch::Flame> flames;  // 2.6.12: the game's own, the torches off (noteCritterFlame)
    flames.swap(critterFlames_);
    const critters::Switches on = critterSwitches();
    if (!crittersVs_ || !crittersPs_ || !on.any() || !cameraValid_ || !target_.texture || !depthViews()) return;
    const float now = waterTime();
    critters::Frame f;
    f.now = double(now);
    f.dt = crittersTime_ > 0.0f ? std::clamp(now - crittersTime_, 0.0f, 0.1f) : 0.0f;
    crittersTime_ = now;
    const float* m = viewToWorld_;
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    for (const torch::Flame& fl : flames) {
        critters::Point at;
        if (fires.size() < 32 && critters::fireOf(fl, proj, m, at)) fires.push_back(at);
    }
    if (cameraTargetZ_ > 0.0f) {  // the point the camera looks at: the player
        const float tz = cameraTargetZ_;
        f.centre = critters::Point{tz * m[6] + m[9], tz * m[7] + m[10], tz * m[8] + m[11]};
        f.centreKnown = true;
    } else {  // else the feet nearest the middle of the screen (the view's z axis)
        float nearest = 1e30f;
        for (const map::Point& p : frameFeet_) {
            const float x = p.x - m[9], y = p.y - m[10], z = p.z - m[11];
            const float along = x * m[6] + y * m[7] + z * m[8];
            const float off = x * x + y * y + z * z - along * along;
            if (along > 0.0f && off < nearest) {
                nearest = off;
                f.centre = critters::Point{p.x, p.y, p.z};
                f.centreKnown = true;
            }
        }
    }
    if (!f.centreKnown) return;
    float best = 1e30f;
    for (const map::Point& p : frameFeet_) {  // the player's feet: the ground's height there
        f.feet.push_back(critters::Point{p.x, p.y, p.z});
        const float dist = std::hypot(p.x - f.centre.x, p.z - f.centre.z);
        if (dist < best && dist < 6.0f) {
            best = dist;
            f.centre.y = p.y;
        }
    }
    for (const footsteps::Step& s : footTracker_.steps())
        if (s.born > crittersStepsSeen_) f.steps.push_back(critters::Point{s.x, s.y, s.z});
    for (const footsteps::Step& s : footTracker_.steps()) crittersStepsSeen_ = std::max(crittersStepsSeen_, s.born);
    f.fires = std::move(fires);
    f.crowns = std::move(crowns);
    f.waterKnown = waterLevelKnown_;
    f.waterLevel = waterLevel_;
    const int minute = effects_.dayNow >= 0 ? effects_.dayNow : living::localMinute();  // 2.6.11
    f.day = living::daytime(minute);
    f.night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    critterWorld_.update(on, f);
    const std::vector<critters::Sprite>& all = critterWorld_.sprites();
    const bool shafts = effects_.underwater && place_.underwater;
    if (all.empty() && !shafts) return;
    if (!crittersData_) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = UINT(critters::kMaxSprites * 4);
        d.Height = 1;
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &crittersData_)) ||
            FAILED(device_->CreateShaderResourceView(crittersData_, nullptr, &crittersDataSrv_))) {
            drop(crittersData_);
            LOG_ERROR("living creatures: no memory for their list (Direct3D 11): none");
            drop(crittersPs_);  // not tried again
            return;
        }
    }
    // The world back into the view: the inverse of viewToWorld's 3 x 3 (its rows the view's axes in the world).
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c3 * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * h - d * k) / det, (a * k - c3 * h) / det, (c3 * d - a * g) / det,
                          (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    const float k0 = float(scale_);
    const auto toView = [&](const critters::Point& w, float out[3]) {
        const float x = w.x - m[9], y = w.y - m[10], z = w.z - m[11];
        out[0] = x * inv[0] + y * inv[3] + z * inv[6];
        out[1] = x * inv[1] + y * inv[4] + z * inv[7];
        out[2] = x * inv[2] + y * inv[5] + z * inv[8];
    };
    const auto toScreen = [&](const float v[3], float& sx, float& sy) {
        sx = (v[0] / v[2] * proj.f0 + proj.cx) * k0 + 0.5f;
        sy = (v[1] / v[2] * proj.f1 + proj.cy) * k0 + 0.5f;
    };
    const float squash = std::clamp(std::fabs(m[7]), 0.2f, 1.0f);  // a circle on the ground seen from the camera
    const int w = target_.width, ht = target_.height;
    static float data[critters::kMaxSprites * 4][4];
    std::memset(data, 0, sizeof data);
    size_t n = 0;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (const critters::Sprite& s : all) {
        if (n >= critters::kMaxSprites) break;
        float v[3];
        toView(s.at, v);
        if (v[2] < proj.nearZ + 1.0f) continue;
        float sx, sy;
        toScreen(v, sx, sy);
        const float size = s.size * proj.f0 * k0 / v[2];
        if (sx < -4.0f * size || sy < -4.0f * size || sx > float(w) + 4.0f * size || sy > float(ht) + 4.0f * size)
            continue;
        float* t0 = data[4 * n];
        t0[0] = sx;
        t0[1] = sy;
        t0[2] = std::max(size, 1.0f);
        t0[3] = v[2];
        float* t1 = data[4 * n + 1];
        t1[0] = float(s.kind);
        t1[1] = s.p1;
        t1[2] = s.p2;
        t1[3] = s.alpha;
        float* t2 = data[4 * n + 2];
        if (s.headingX != 0.0f || s.headingZ != 0.0f) {  // the heading turned to the screen
            float tip[3];
            toView(critters::Point{s.at.x + s.headingX, s.at.y, s.at.z + s.headingZ}, tip);
            if (tip[2] > proj.nearZ) {
                float tx, ty;
                toScreen(tip, tx, ty);
                t2[0] = std::atan2(ty - sy, tx - sx);
            }
        }
        t2[1] = squash;
        if (s.kind == critters::Dragonfly && waterLevelKnown_) {  // over the water only: its point on the surface
            float q[3];
            toView(critters::Point{s.at.x, waterLevel_, s.at.z}, q);
            if (q[2] > proj.nearZ) {
                float* t3 = data[4 * n + 3];
                toScreen(q, t3[0], t3[1]);
                t3[2] = q[2];
                t3[3] = 1.0f;
            }
        }
        x0 = std::min(x0, sx - 3.0f * size);
        y0 = std::min(y0, sy - 3.0f * size);
        x1 = std::max(x1, sx + 3.0f * size);
        y1 = std::max(y1, sy + 3.0f * size);
        ++n;
    }
    if (shafts && n < critters::kMaxSprites) {  // the light from above under the sea, over the whole picture
        data[4 * n][2] = 1.0f;
        data[4 * n + 1][0] = 13.0f;
        data[4 * n + 1][3] = 1.0f;
        ++n;
    }
    if (!n) return;
    D3D11_RECT clip{0, 0, w, ht};
    if (!shafts) {
        clip.left = LONG(std::clamp(x0, 0.0f, float(w)));
        clip.top = LONG(std::clamp(y0, 0.0f, float(ht)));
        clip.right = LONG(std::clamp(x1 + 1.0f, 0.0f, float(w)));
        clip.bottom = LONG(std::clamp(y1 + 1.0f, 0.0f, float(ht)));
        if (clip.right <= clip.left || clip.bottom <= clip.top) return;
    }
    ctx_->UpdateSubresource(crittersData_, 0, nullptr, data, UINT(sizeof data), 0);
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.targetSize[0] = float(w);
    c.targetSize[1] = float(ht);
    c.scale = k0;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(ht);
    c.sharpen = now;
    c.twin[2] = 1.0f - 0.65f * f.night;  // the creatures' light
    if (!crittersLogged_) LOG_INFO("living creatures: on %s (Direct3D 11)", map_.c_str());
    crittersLogged_ = true;
    targetBound_ = false;  // the next draw binds the target, the viewport and the game's shaders again
    ID3D11ShaderResourceView* none[4] = {nullptr, nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 4, none);
    ctx_->OMSetRenderTargets(1, &target_.rtv, nullptr);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(w), float(ht), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    ctx_->RSSetScissorRects(1, &clip);
    setConstants(c);
    ctx_->OMSetBlendState(blendState(true, d3d7::BLEND_ONE, d3d7::BLEND_INVSRCALPHA,
                                     D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                                         D3D11_COLOR_WRITE_ENABLE_BLUE),
                          nullptr, 0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(crittersVs_, nullptr, 0);
    ctx_->PSSetShader(crittersPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    passSamplers();
    ctx_->VSSetShaderResources(1, 1, &crittersDataSrv_);  // the vertex shader places the quads from the list
    ID3D11ShaderResourceView* views[4] = {nullptr, crittersDataSrv_, nullptr, depthSrv_};
    ctx_->PSSetShaderResources(0, 4, views);
    ctx_->Draw(UINT(6 * n), 0);
    ctx_->PSSetShaderResources(0, 4, none);
    ctx_->VSSetShaderResources(1, 1, none);
    const D3D11_RECT whole{0, 0, w, ht};
    ctx_->RSSetScissorRects(1, &whole);
}

// 2.5.5 (actor_shadows.cpp): the way to the map's light from its file, as the client turns it - when its axes are known
bool Renderer11::mapSun(float toSun[3]) const {
    const rsw::World* w = mapWorld_.world();
    const map::Axes axes = mapWorld_.axes();
    if (!w || !w->lightKnown || !axes.known()) return false;
    const float lon = float(w->lightLongitude) * 0.0174533f, lat = float(w->lightLatitude) * 0.0174533f;
    toSun[0] = float(axes.x) * -std::cos(lon) * std::sin(lat);
    toSun[1] = -std::cos(lat);
    toSun[2] = float(axes.z) * -std::sin(lon) * std::sin(lat);
    return true;
}

float Renderer11::sunHeight() const {
    float toSun[3];
    if (!effects_.dayCycle && mapSun(toSun))
        return std::clamp(std::asin(std::clamp(-toSun[1], -1.0f, 1.0f)), 15.0f * 0.0174533f, 70.0f * 0.0174533f);
    const daylight::Light day = daylight::at(effects_.dayCycle ? effects_.dayNow : -1);
    return std::clamp(50.0f / day.length * 0.55f, 15.0f, 70.0f) * 0.0174533f;
}

void Renderer11::sunGround(float out[3]) const {
    float toSun[3];
    if (mapSun(toSun)) {
        const float len = std::sqrt(toSun[0] * toSun[0] + toSun[2] * toSun[2]);
        if (len > 1e-4f) {
            out[0] = -toSun[0] / len;
            out[1] = 0.0f;
            out[2] = -toSun[2] / len;
            return;
        }
    }
    const daylight::Light l = daylight::at(effects_.dayCycle ? effects_.dayNow : -1);
    const float len = std::max(std::sqrt(l.shadow[0] * l.shadow[0] + l.shadow[1] * l.shadow[1]), 1e-4f);
    out[0] = l.shadow[0] / len;
    out[1] = 0.0f;
    out[2] = l.shadow[1] / len;
}

void Renderer11::sunOnScreen(float out[2]) const {
    const daylight::Light l = daylight::at(effects_.dayCycle ? effects_.dayNow : -1);
    out[0] = l.shadow[0];
    out[1] = l.shadow[1];
    if (!cameraValid_) return;
    float g[3];
    sunGround(g);
    const float* m = viewToWorld_;
    const float sx = g[0] * m[0] + g[1] * m[1] + g[2] * m[2], sy = g[0] * m[3] + g[1] * m[4] + g[2] * m[5];
    const float len = std::sqrt(sx * sx + sy * sy);
    if (len < 1e-4f) return;
    out[0] = sx / len;
    out[1] = sy / len;
}

void Renderer11::spriteConstants(Constants& c, int gameWidth) const {  // 2.5.5 (sun_map.cpp)
    c.stage0[1] |= 1u << 28;
    float sun[2];
    sunOnScreen(sun);
    const float height = outdoors_ ? sunHeight() : 1.2f;
    c.twin[0] = -sun[0];
    c.twin[1] = -sun[1];
    c.twin[2] = std::sin(height) * 0.6f;
    const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    c.uiPixels[0] = float(std::clamp(effects_.spriteLightStrength, 0, 200)) / 100.0f;
    c.uiPixels[1] = outdoors_ ? 1.0f - 0.8f * night : 0.3f;
    c.uiPixels[2] = float(gameWidth);  // 2.6.3: its edge in the game's texels, also under an AI picture
}

// 2.5.5 (weather.cpp updateWind): the shared wind now and what it has carried since the last frame
void Renderer11::updateWind() {
    const float now = waterTime();
    float dt = windClock_ < 0.0f ? 0.0f : now - windClock_;
    if (dt < 0.0f) dt = 0.0f;
    windClock_ = now;
    windNow_ = wind::at(double(now));
    const float* m = viewToWorld_;
    const float axis = std::sqrt(m[0] * m[0] + m[2] * m[2]);
    windScreenX_ = cameraValid_ && axis > 1e-4f ? (windNow_.x * m[0] + windNow_.z * m[2]) / axis : 0.0f;
    const float s = windNow_.strength;
    weatherDrift_.advance(windScreenX_ * s * 60.0f, 0.0f, dt);
    skyDrift_.advance(windNow_.x * s * 0.012f, windNow_.z * s * 0.012f, dt);
}

// 2.5.5: as Direct3D 12's applyShafts (haze.cpp) - the light in the air about this frame's lights (the torches'
// pass's list, lightData_) and, under the open sky, rays from where the sky shows past the map's edge, added
void Renderer11::applyShafts() {
    EffectSpan span(*this, kTimeShafts11);
    shaftsDone_ = true;
    if (!shaftsPs_ || !halfOverPs_ || !effects_.shafts || effects_.shaftsStrength <= 0 || !cameraValid_ ||
        !target_.texture || !depthViews() || (shaftLights_ <= 0 && !(outdoors_ && effects_.skyRays)))
        return;
    const int w = target_.width, h = target_.height;
    const float user = float(std::clamp(effects_.shaftsStrength, 0, 200)) / 100.0f;
    const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);
    c.scale = float(scale_);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = float(shaftLights_);
    c.twin[0] = 0.22f * user * (1.0f + 0.6f * night);  // the glow shows more in the dark
    c.twin[1] = outdoors_ && effects_.skyRays ? 0.5f * user * (1.0f - night) : 0.0f;
    float sun[2];
    sunOnScreen(sun);
    c.sourceRect[0] = float(w) * (0.5f - sun[0] * 1.6f);
    c.sourceRect[1] = float(h) * (0.5f - sun[1] * 1.6f);
    c.sourceRect[2] = 1.0f;
    // 2.5.7: at half the size into a target of its own (a quarter of the rays' depth reads), added over smoothly
    const int hw = std::max(1, (w + 1) / 2), hh = std::max(1, (h + 1) / 2);
    if (shafts_.width != hw || shafts_.height != hh) {
        dropTarget(shafts_);
        if (!makeTarget(shafts_, hw, hh)) {
            LOG_ERROR("light in the air: no video memory at %dx%d (Direct3D 11): none", hw, hh);
            shaftsPs_ = nullptr;  // not tried again (the object stays until the end)
            return;
        }
    }
    c.sourceRect[3] = 2.0f;
    const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx_->ClearRenderTargetView(shafts_.rtv, none);
    maskPass(shaftsPs_, shafts_, c, nullptr, depthSrv_, nullptr, shaftLights_ > 0 ? lightDataSrv_ : nullptr);
    worldPass(halfOverPs_, c, 0, 3, nullptr, shafts_.srv);
}

// 2.5.5: as Direct3D 12's applyFlare (haze.cpp) - where the sky shows towards the sun, a glow and rings, added
void Renderer11::applyFlare() {
    flareDone_ = true;
    if (!flarePs_ || !effects_.flare || !outdoors_ || effects_.nightNow >= 60 || !target_.texture || !depthViews())
        return;
    const int w = target_.width, h = target_.height;
    const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    Constants c{};
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    float sun[2];
    sunOnScreen(sun);
    c.sourceRect[0] = float(w) * (0.5f - sun[0] * 1.6f);
    c.sourceRect[1] = float(h) * (0.5f - sun[1] * 1.6f);
    c.twin[0] = 0.35f * (1.0f - night / 0.6f);
    worldPass(flarePs_, c, 0, 3, depthSrv_);
}

float Renderer11::mapFadeNow() {
    if (!effects_.mapFade || fadeStart_ < 0.0f) return 1.0f;
    const float t = (waterTime() - fadeStart_) / 0.7f;
    return t >= 1.0f ? 1.0f : t <= 0.0f ? 0.0f : t * t * (3.0f - 2.0f * t);
}

// 2.5.2: as Direct3D 12's applyBloom (ao.cpp) - the bright parts at a quarter of the size (bloom_down_ps), blurred
// across and down twice (dof_blur_ps), added onto the picture with the map's fade
// (bloom_apply_ps, from the picture's copy).
void Renderer11::applyBloom() {
    EffectSpan span(*this, kTimeBloom11);
    bloomDone_ = true;
    if (!bloomWanted() || !target_.texture) return;
    const int w = target_.width, h = target_.height;
    const int qw = std::max(1, (w + 3) / 4), qh = std::max(1, (h + 3) / 4);
    for (Target& t : bloom_)
        if (t.width != qw || t.height != qh) {
            dropTarget(t);
            if (!makeTarget(t, qw, qh)) {
                LOG_ERROR("bloom: no video memory at %dx%d (Direct3D 11): it stays off", qw, qh);
                bloomApplyPs_ = nullptr;  // not tried again
                return;
            }
        }
    if (!copyScene()) return;
    Constants c{};
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[2] = float(qw);
    c.uiPixels[3] = float(qh);
    c.footprint = 0.88f;  // the bright parts: over this
    const bool bloom = effects_.bloom && effects_.bloomStrength > 0;
    c.sharpen = bloom ? 0.6f * float(std::clamp(effects_.bloomStrength, 0, 200)) / 100.0f : 0.0f;
    c.twin[3] = mapFadeNow();
    if (bloom) {
        maskPass(bloomDownPs_, bloom_[0], c, nullptr, nullptr, scene_.srv);
        for (int k = 0; k < 4; ++k) {  // across, down, then again twice as wide
            const int i = k % 2;
            c.sourceRect[0] = i == 0 ? float(1 + k / 2) : 0.0f;
            c.sourceRect[1] = i == 0 ? 0.0f : float(1 + k / 2);
            maskPass(dofBlurPs_, bloom_[1 - i], c, bloom_[i].srv, nullptr);
        }
    }
    if (!bloomLogged_) LOG_INFO("bloom: %dx%d for the %dx%d picture (Direct3D 11)", qw, qh, w, h);
    bloomLogged_ = true;
    worldPass(bloomApplyPs_, c, 2, 0, nullptr, bloom ? bloom_[0].srv : nullptr);
}

// 2.5.1: the soft shadows (ao_ps, ao_blur_ps, ao_apply_ps) as Direct3D 12's ao.cpp draws them - a mask at a half
// (a quarter of a big picture) of the size from the depth, blurred across and down, multiplied into the picture.
void Renderer11::applyAo() {
    EffectSpan span(*this, kTimeAo11);
    aoDone_ = true;
    if (!aoWanted() || !target_.texture || !depthViews()) return;
    const int w = target_.width, h = target_.height;
    const int div = w >= 3000 ? 4 : 2;
    const int aw = std::max(1, (w + div - 1) / div), ah = std::max(1, (h + div - 1) / div);
    for (Target& t : aoTarget_)
        if (t.width != aw || t.height != ah) {
            dropTarget(t);
            if (!makeTarget(t, aw, ah, DXGI_FORMAT_R8_UNORM)) {
                LOG_ERROR("shadows: no video memory for them at %dx%d (Direct3D 11)", aw, ah);
                aoApplyPs_ = nullptr;  // not tried again (the object stays until the end)
                return;
            }
        }
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(scale_);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[2] = float(aw);
    c.uiPixels[3] = float(ah);
    c.footprint = 1.5f + 7.5f * float(std::clamp(effects_.aoSize, 0, 100)) / 100.0f;  // world units
    c.sharpen = float(std::clamp(effects_.aoStrength, 0, 100)) / 100.0f;
    maskPass(aoPs_, aoTarget_[0], c, nullptr, depthSrv_);
    for (int i = 0; i < 2; ++i) {  // across, then down
        c.sourceRect[0] = i == 0 ? 1.0f : 0.0f;
        c.sourceRect[1] = i == 0 ? 0.0f : 1.0f;
        maskPass(aoBlurPs_, aoTarget_[1 - i], c, aoTarget_[i].srv, depthSrv_);
    }
    if (!aoLogged_) LOG_INFO("shadows: %dx%d, from the depth of the %dx%d picture (Direct3D 11)", aw, ah, w, h);
    aoLogged_ = true;
    worldPass(aoApplyPs_, c, false, 2, nullptr, aoTarget_[0].srv);
}

// 2.5: the low mists (mist_ps) as Direct3D 12's applyMist (haze.cpp) - the morning's over the fields and the towns,
// a swamp's - at the ground's height at the character (2.6.8: the far haze and the dungeons' dark and fog gone).
void Renderer11::applyMist() {
    EffectSpan span(*this, kTimeMist11);
    mistDone_ = true;
    const living::Place place = living::placeOf(map_, outdoors_, season_ == trees::Season::Autumn);
    const int minute = effects_.dayNow >= 0 ? effects_.dayNow : living::localMinute();  // 2.6.11
    const float morning = effects_.morningFog && outdoors_ && !desert_ ? living::morningFog(minute) : 0.0f;
    const bool swampy = effects_.swamp && place.swamp;
    if (!mistPs_ || (morning <= 0.0f && !swampy) || !cameraValid_ || !target_.texture || !depthViews()) return;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);
    c.scale = float(scale_);
    c.uiPixels[0] = float(target_.width);
    c.uiPixels[1] = float(target_.height);
    if (swampy) {  // 2.5.3 (haze.cpp 1.7): a swamp's mist, always
        static const float kMurk[3] = {0.62f, 0.68f, 0.55f};
        std::copy(kMurk, kMurk + 3, c.sourceRect);
        c.sourceRect[3] = 0.3f;
    }
    if (morning * 0.6f > c.sourceRect[3]) {  // the morning's, thicker
        static const float kMist[3] = {0.84f, 0.87f, 0.9f};
        std::copy(kMist, kMist + 3, c.sourceRect);
        c.sourceRect[3] = std::min(1.0f, morning * 0.6f);
    }
    if (cameraTargetZ_ > 0.0f) {  // the low fog at the ground's height at the character
        c.sourceClamp[0] = camera::targetHeight(viewToWorld_, cameraTargetZ_);
        c.sourceClamp[1] = 1.0f;
    }
    c.sharpen = waterTime();
    if (!mistLogged_)
        LOG_INFO("mists: %s over %s (Direct3D 11)", swampy ? "a swamp's" : "the morning's", map_.c_str());
    mistLogged_ = true;
    worldPass(mistPs_, c, false, 1, depthSrv_);
}

// 2.5: the sky (sky_ps) as Direct3D 12's applySky - past the map's edge under the open sky, its clouds and stars,
// land below the horizon when asked (the picture's copy then).
void Renderer11::applySky() {
    EffectSpan span(*this, kTimeSky11);
    skyDone_ = true;
    if (!skyPs_ || !effects_.sky || effects_.skyStrength <= 0 || !outdoors_ || !cameraValid_ || !target_.texture ||
        !depthViews())
        return;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);
    c.scale = float(scale_);
    c.uiPixels[0] = float(target_.width);
    c.uiPixels[1] = float(target_.height);
    const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    const float pale[3] = {0.72f, 0.80f, 0.91f}, deep[3] = {0.33f, 0.52f, 0.86f}, dark[3] = {0.03f, 0.05f, 0.11f};
    for (int k = 0; k < 3; ++k) {
        const float horizon = pale[k];  // 2.6.8: the far haze's colour gone with it
        c.fogColor[k] = horizon + (dark[k] * 2.0f - horizon) * night;
        c.sourceRect[k] = deep[k] + (dark[k] - deep[k]) * night;
    }
    c.twin[0] = float(std::clamp(effects_.skyClouds, 0, 100)) / 100.0f;
    c.twin[1] = float(std::clamp(effects_.skyStrength, 0, 100)) / 100.0f;
    c.twin[2] = night;
    c.sourceClamp[3] = effects_.stars ? 1.0f : 0.0f;
    if (windShared()) {  // 2.5.5: the clouds drift with the shared wind
        c.sourceClamp[1] = skyDrift_.x;
        c.sourceClamp[2] = skyDrift_.y + 1e-3f;
    }
    const bool land = effects_.edgeLand && cameraTargetZ_ > 0.0f;
    if (land) {
        c.twin[3] = 1.0f;
        c.sourceClamp[0] = camera::targetHeight(viewToWorld_, cameraTargetZ_);
    }
    c.sharpen = waterTime();
    if (!skyLogged_) LOG_INFO("sky: over %s (Direct3D 11)", map_.c_str());
    skyLogged_ = true;
    worldPass(skyPs_, c, land, 1, depthSrv_);
}

// 2.5: the weather (weather_ps) as Direct3D 12's weather.cpp - the map's snow, rain, leaves or sand in three layers,
// or the motes (the pollen outdoors, the dust indoors); the shared wind and the lit dust stay Direct3D 12's.
void Renderer11::applyWeather() {
    EffectSpan span(*this, kTimeWeather11);
    weatherDone_ = true;
    const bool own = effects_.weather && effects_.weatherAmount > 0 && weather_ != weather::Kind::None;
    const bool motes = !own && effects_.motes && effects_.motesAmount > 0 && !map_.empty() &&
                       (outdoors_ ? effects_.pollen : effects_.indoorDust);
    if (!weatherPs_ || (!own && !motes) || !target_.texture) return;
    Constants c{};
    c.scale = float(scale_);
    c.uiPixels[0] = float(target_.width);
    c.uiPixels[1] = float(target_.height);
    c.twin[0] = float(int(weather_));
    c.footprint = float(std::clamp(effects_.weatherAmount, 0, 200)) / 100.0f;
    if (motes) {  // 0.70: kinds 5 (pollen) and 6 (dust); the pollen fewer at night
        const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
        c.twin[0] = outdoors_ ? 5.0f : 6.0f;
        c.footprint = float(std::clamp(effects_.motesAmount, 0, 200)) / 100.0f;
        if (outdoors_) c.footprint *= 1.0f - 0.7f * night;
    }
    c.sharpen = waterTime();
    if (cameraValid_) {  // the camera's place along the view's own across and down, in game pixels nearby
        const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
        const float* m = viewToWorld_;
        const float perUnit = proj.f0 / 250.0f;
        c.sourceRect[0] = (m[9] * m[0] + m[10] * m[1] + m[11] * m[2]) * perUnit;
        c.sourceRect[1] = (m[9] * m[3] + m[10] * m[4] + m[11] * m[5]) * perUnit;
    }
    if (!weatherLogged_)
        LOG_INFO("weather: %s over %s (Direct3D 11)",
                 motes ? (outdoors_ ? "pollen" : "dust")
                       : weather_ == weather::Kind::Snow   ? "snow"
                         : weather_ == weather::Kind::Rain ? "rain"
                         : weather_ == weather::Kind::Leaves ? "leaves"
                                                           : "sand",
                 map_.c_str());
    weatherLogged_ = true;
    if (windShared()) {  // 2.5.5: blown along the screen by the shared wind
        c.sourceRect[2] = weatherDrift_.x;
        c.sourceRect[3] = windScreenX_ * windNow_.strength * 60.0f;
    }
    worldPass(weatherPs_, c, false, 1);
    // 2.5.3 (weather.cpp 0.64): the rain's splashes on the ground, where it is known (the camera read)
    if (motes || weather_ != weather::Kind::Rain || !effects_.rainSplashes || !cameraValid_ || !splashPs_ ||
        !depthViews())
        return;
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);
    worldPass(splashPs_, c, 0, 1, depthSrv_);
}

void Renderer11::saveScreenshot(const std::string& path) {
    ID3D11Texture2D* back = nullptr;
    if (!swap_ || FAILED(swap_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) return;
    D3D11_TEXTURE2D_DESC d{};
    back->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    bool ok = false;
    if (SUCCEEDED(device_->CreateTexture2D(&d, nullptr, &staging))) {
        ctx_->CopyResource(staging, back);
        D3D11_MAPPED_SUBRESOURCE m{};
        if (SUCCEEDED(ctx_->Map(staging, 0, D3D11_MAP_READ, 0, &m))) {
            ok = writePng(path, static_cast<const uint8_t*>(m.pData), int(d.Width), int(d.Height), m.RowPitch);
            ctx_->Unmap(staging, 0);
        }
    }
    drop(staging);
    drop(back);
    if (ok)
        LOG_INFO("screenshot: %s (%ux%u)", path.c_str(), d.Width, d.Height);
    else
        LOG_ERROR("screenshot: %s could not be saved", path.c_str());
}

void Renderer11::finish() {
    if (ctx_) ctx_->Flush();
}

bool Renderer11::startAi(const std::string& gameDir, const AiSettings& settings) {
    if (ai_ || !device_) return ai_ != nullptr;
    IDXGIDevice* dxgi = nullptr;
    IDXGIAdapter* adapter = nullptr;
    if (SUCCEEDED(device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi))))
        dxgi->GetAdapter(&adapter);
    drop(dxgi);
    ai_ = new Ai11(device_, ctx_, adapter);
    drop(adapter);
    ai_->setIconSink(iconSink_, iconContext_);
    std::string error;
    if (!ai_->start(gameDir, settings, error)) {
        LOG_ERROR("AI textures: %s; they stay off", error.c_str());
        delete ai_;
        ai_ = nullptr;
        return false;
    }
    return true;
}

void Renderer11::setAiShown(uint8_t kinds, uint8_t sprites) {
    if (ai_) {
        ai_->shown = kinds != 0;
        ai_->kindsShown = kinds;
        ai_->spritesShown = sprites;
    }
}

void Renderer11::requestIcon(uint64_t key, int width, int height, const uint32_t* argb) {
    if (ai_) ai_->noteIcon(key, width, height, argb);
}

void Renderer11::setIconSink(Renderer::IconSink sink, void* context) {
    iconSink_ = sink;
    iconContext_ = context;
    if (ai_) ai_->setIconSink(sink, context);
}

void Renderer11::timeBegin() {
    if (!timing_ || untimed_) return;
    FrameTimes& f = times_[timeSlot_];
    if (!f.open) {  // the frame's first work
        if (!readTimes(timeSlot_, false)) {  // the slot's frame of 4 frames ago is still on the GPU
            untimed_ = true;
            return;
        }
        ctx_->Begin(f.disjoint);
        f.open = true;
        f.used = 0;
        f.effectBegun = f.effectEnded = 0;  // 0.89
    }
    if (f.inSpan || f.used + 2 > kStamps) return;  // no stamps left: timeEnd stretches the last span
    ctx_->End(f.stamps[f.used]);
    f.inSpan = true;
}

void Renderer11::timeEnd() {
    if (!timing_ || untimed_) return;
    FrameTimes& f = times_[timeSlot_];
    if (!f.open) return;
    if (f.inSpan) {
        ctx_->End(f.stamps[f.used + 1]);
        f.used += 2;
        f.inSpan = false;
    } else if (f.used >= 2) {
        ctx_->End(f.stamps[f.used - 1]);  // the last span ends here now
    }
}

void Renderer11::timeFrame() {
    if (!timing_) return;
    FrameTimes& f = times_[timeSlot_];
    if (!untimed_ && f.open) {
        if (f.inSpan) timeEnd();
        ctx_->End(f.disjoint);
        f.open = false;
        f.pending = true;
        timeSlot_ = (timeSlot_ + 1) % kTimedFrames;
    }
    untimed_ = false;
    const ULONGLONG now = GetTickCount64();
    if (!recentSince_) recentSince_ = now;
    if (now - recentSince_ >= 500 && recentFrames_) {  // 2.2.1: twice a second, for the frame counter
        static const char* const kPasses[kEffectTimes11] = {"actor_shadows", "lights", "night", "grade",
                                                             "mist", "sky", "weather", "ao", "bloom",
                                                             "steps", "heat", "shafts",
                                                             "glow", "water", "floor", "grass", "world",
                                                             "interface"};
        recent_.gpuMs = recentMs_ / recentFrames_;
        recent_.effects.clear();
        for (int e = 0; e < kEffectTimes11; ++e) {
            // the effects' costs (keep_fps, the console) and the world's (2.5.16: keep_fps turns the world's
            // extras off when it is the world that is slow), not the interface's
            if (recentEffectMs_[e] > 0.0 && e != kTimeUi11)
                recent_.effects[kPasses[e]] = recentEffectMs_[e] / recentFrames_;
            recentEffectMs_[e] = 0.0;
        }
        ++recent_.serial;
        recentMs_ = 0.0;
        recentFrames_ = 0;
        recentSince_ = now;
    }
    if (!gpuSince_) gpuSince_ = now;
    if (now - gpuSince_ >= 30000) logGpu(now);
}

// A frame's spans summed into the count; false while its work is still on the GPU (wait: up to 0.1 s for it).
bool Renderer11::readTimes(UINT slot, bool wait) {
    FrameTimes& f = times_[slot];
    if (!f.pending) return true;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{};
    HRESULT hr = ctx_->GetData(f.disjoint, &d, sizeof d, wait ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH);
    for (int i = 0; wait && hr == S_FALSE && i < 100; ++i) {
        Sleep(1);
        hr = ctx_->GetData(f.disjoint, &d, sizeof d, 0);
    }
    if (hr == S_FALSE) return false;
    f.pending = false;
    if (FAILED(hr) || d.Disjoint || !d.Frequency) return true;  // the GPU's clock changed meanwhile: not counted
    UINT64 ticks = 0;
    for (UINT i = 0; i + 1 < f.used; i += 2) {
        UINT64 a = 0, b = 0;
        if (ctx_->GetData(f.stamps[i], &a, sizeof a, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            ctx_->GetData(f.stamps[i + 1], &b, sizeof b, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            return true;
        if (b > a) ticks += b - a;
    }
    gpuMs_ += double(ticks) * 1000.0 / double(d.Frequency);
    ++gpuFrames_;
    recentMs_ += double(ticks) * 1000.0 / double(d.Frequency);  // 2.2.1
    ++recentFrames_;
    for (int e = 0; e < kEffectTimes11; ++e) {  // 0.89: the effects' own spans
        if (!(f.effectEnded >> e & 1)) continue;
        UINT64 a = 0, b = 0;
        if (ctx_->GetData(f.effect[e][0], &a, sizeof a, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
            ctx_->GetData(f.effect[e][1], &b, sizeof b, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK && b > a) {
            effectMs_[e] += double(b - a) * 1000.0 / double(d.Frequency);
            recentEffectMs_[e] += double(b - a) * 1000.0 / double(d.Frequency);
        }
    }
    return true;
}

// 2.5.15: the world's span from the frame's first world draw to its first interface draw (before: true there), the
// interface's from after the world's effects drawn at that draw (before: false) to the present - what the gathered
// draws so far did goes before each stamp; the world's effects fall between the two (their own spans tell them).
void Renderer11::timeDraws(bool ui, bool before) {
    if (!timing_ || untimed_) return;
    const FrameTimes& f = times_[timeSlot_];
    if (!f.open) return;
    const bool worldBegun = (f.effectBegun >> kTimeWorld11 & 1) != 0, uiBegun = (f.effectBegun >> kTimeUi11 & 1) != 0;
    if (!ui && !worldBegun && !uiBegun) {
        flushBatch();
        effectBegin(kTimeWorld11);
    } else if (ui && before && worldBegun && !(f.effectEnded >> kTimeWorld11 & 1)) {
        flushBatch();
        effectEnd(kTimeWorld11);
    } else if (ui && !before && !uiBegun) {
        flushBatch();
        effectBegin(kTimeUi11);
    }
}

void Renderer11::effectBegin(EffectTime11 e) {
    if (!timing_ || untimed_) return;
    FrameTimes& f = times_[timeSlot_];
    if (!f.open || (f.effectBegun >> e & 1)) return;
    ctx_->End(f.effect[e][0]);
    f.effectBegun |= 1u << e;
}

void Renderer11::effectEnd(EffectTime11 e) {
    if (!timing_ || untimed_) return;
    FrameTimes& f = times_[timeSlot_];
    if (!f.open || !(f.effectBegun >> e & 1) || (f.effectEnded >> e & 1)) return;
    ctx_->End(f.effect[e][1]);
    f.effectEnded |= 1u << e;
}

void Renderer11::logGpu(ULONGLONG now) {
    if (gpuFrames_ && gpuSince_) {
        LOG_INFO("GPU: %.2f ms of work a frame (%u frames in %.0f s)", gpuMs_ / gpuFrames_, gpuFrames_,
                 double(now - gpuSince_) / 1000.0);
        // 0.89: the effects one by one, as Direct3D 12's log has them
        static const char* const kNames[kEffectTimes11] = {"characters' shadows", "torches' light", "night",
                                                            "colour", "mists", "sky", "weather", "soft shadows",
                                                            "bloom", "footprints", "heat haze",
                                                            "light in the air",
                                                            "glow", "water's copies",
                                                            "floor reflections", "grass", "", ""};
        if (effectMs_[kTimeWorld11] > 0.0 || effectMs_[kTimeUi11] > 0.0)  // 2.5.15
            LOG_INFO("GPU: the game's world %.2f ms a frame (the effects drawn amid it too), its interface %.2f ms",
                     effectMs_[kTimeWorld11] / gpuFrames_, effectMs_[kTimeUi11] / gpuFrames_);
        std::string effects;
        for (int e = 0; e < kEffectTimes11; ++e) {
            if (effectMs_[e] <= 0.0 || drawsTimer(e)) continue;
            char part[64];
            std::snprintf(part, sizeof part, "%s%s %.2f ms", effects.empty() ? "" : ", ", kNames[e],
                          effectMs_[e] / gpuFrames_);
            effects += part;
        }
        if (!effects.empty()) LOG_INFO("GPU: effects a frame - %s", effects.c_str());
    }
    // 0.89: what the frames asked of the processor and the driver
    if (stats_.frames && gpuSince_) {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        const double n = double(stats_.frames), seconds = double(now - gpuSince_) / 1000.0;
        LOG_INFO("Direct3D 11: %.0f draws a frame in %.0f draw calls, %.0f calls to the driver (%.0f spared), %.2f "
                 "ms of the processor in drawing; the game's textures %llu made, %llu reused, %llu freed, %u alive "
                 "(%u MB kept for reuse); %.1f MB uploaded a second",
                 double(stats_.draws) / n, double(stats_.batches) / n, double(stats_.calls) / n,
                 double(stats_.spared) / n,
                 double(stats_.ticks) * 1000.0 / double(freq.QuadPart) / n,
                 static_cast<unsigned long long>(stats_.made), static_cast<unsigned long long>(stats_.reused),
                 static_cast<unsigned long long>(stats_.freed), static_cast<unsigned>(textures_.size()),
                 static_cast<unsigned>(pooledBytes_ >> 20),
                 seconds > 0 ? double(stats_.uploaded) / 1048576.0 / seconds : 0.0);
    }
    stats_ = Stats{};
    for (double& ms : effectMs_) ms = 0.0;
    gpuMs_ = 0.0;
    gpuFrames_ = 0;
    gpuSince_ = now;
}

}  // namespace uagfx::gpu
