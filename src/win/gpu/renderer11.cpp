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
#include "core/foliage.hpp"
#include "core/game_camera.hpp"
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
    if (ctx_) ctx_->ClearState();
    releaseSwapChain();
    for (auto& t : textures_) {
        dropMaterial(t.second);  // 1.14
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
    } wants[22] = {{"draw_vs", vs, shaderSource(), {}},    {"draw_ps", ps, shaderSource(), {}},
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
                   {"material_ps", ps, shaderSource(), {}, true}};  // 1.14
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
            HRESULT hr = E_FAIL;
            std::string said;
            for (int i = 0; i < 4 && FAILED(hr); ++i) {
                ID3DBlob* code = nullptr;
                ID3DBlob* errors = nullptr;
                hr = compile(w.source, std::strlen(w.source), "uaro-graphics.hlsl", i == 3 ? kSafe : nullptr,
                             nullptr, w.entry, w.profile, kLevels[i], 0, &code, &errors);
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
        const std::vector<uint8_t> bytes = shader_cache::pack(key, all);
        const std::string part = cacheFile + "." + std::to_string(GetCurrentProcessId()) + ".tmp";
        std::ofstream f(part, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        f.close();
        if (!f || !MoveFileExA(part.c_str(), cacheFile.c_str(), MOVEFILE_REPLACE_EXISTING)) DeleteFileA(part.c_str());
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
    return true;
}

void Renderer11::dropTarget(Target& t) {
    drop(t.srv);
    drop(t.rtv);
    drop(t.texture);
    t.width = t.height = 0;
}

bool Renderer11::makeDepth(int width, int height, ID3D11Texture2D*& depth, ID3D11DepthStencilView*& dsv) {
    D3D11_TEXTURE2D_DESC d{};
    d.Width = UINT(width);
    d.Height = UINT(height);
    d.MipLevels = d.ArraySize = 1;
    // 0.60.1: readable too (the characters' shadows read the ground's depth): typeless, a D32 view to draw with
    d.Format = DXGI_FORMAT_R32_TYPELESS;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = device_->CreateTexture2D(&d, nullptr, &depth);
    D3D11_DEPTH_STENCIL_VIEW_DESC v{};
    v.Format = DXGI_FORMAT_D32_FLOAT;
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

void Renderer11::releaseTexture(uint32_t id) {  // Direct3D 11 keeps it while queued work still reads it
    const auto it = textures_.find(id);
    if (it == textures_.end()) return;
    if (ai_) ai_->forget(id);
    objectTextures_.forget(id);  // 0.61
    ++stats_.freed;
    // 0.89: a small one kept for the game's next of its size (32 MB of them at most) - a new surface's levels are
    // uploaded whole before it is first drawn (TextureLevels starts with all of them to upload), the old picture gone
    dropMaterial(it->second);  // 1.14
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
bool Renderer11::uploadGeometry(const FrameRecord& rec) {
    const auto fill = [this](ID3D11Buffer*& buffer, UINT& capacity, UINT bind, const void* data, size_t bytes,
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
                    for (size_t at : {size_t(16), size_t(20)}) std::swap(v[d.vertexOffset + i * stride + at],
                                                                         v[d.vertexOffset + i * stride + at + 2]);
            }
        }
        ctx_->Unmap(buffer, 0);
        return true;
    };
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const std::vector<uint16_t>& i = rec.indices();
    return fill(vertices_, vertexBytes_, D3D11_BIND_VERTEX_BUFFER, v.data(), v.size(),
                bgraVertices_ ? nullptr : &rec) &&
           fill(indices_, indexBytes_, D3D11_BIND_INDEX_BUFFER, i.data(), i.size() * sizeof(uint16_t), nullptr);
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

ID3D11RasterizerState* Renderer11::rasterState(uint32_t cull) {
    const auto it = rasters_.find(cull);
    if (it != rasters_.end()) return it->second;
    D3D11_RASTERIZER_DESC d{};
    d.FillMode = D3D11_FILL_SOLID;
    d.CullMode = D3D11_CULL_MODE(cull);
    d.FrontCounterClockwise = FALSE;
    d.DepthClipEnable = TRUE;
    d.ScissorEnable = TRUE;  // the game's viewport clips its pre-transformed vertices
    ID3D11RasterizerState* s = nullptr;
    const HRESULT hr = device_->CreateRasterizerState(&d, &s);
    if (FAILED(hr)) failed("a rasterizer state", hr);
    rasters_.emplace(cull, s);
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
    if (level == 0 && u.rect.left == 0 && u.rect.top == 0 && u.rect.right == t.width && u.rect.bottom == t.height)
        it->second.leafy = foliage::leafy(rec.pixels().data() + u.pixels, t.width, t.height);  // 0.64
    if (u.slotCount) noteCells(it->second, rec.slots().data() + u.firstSlot, u.slotCount);  // 0.60.1: the shadows
    if (ai_)  // 0.47: its key, or the sprite cache's new frames with their sources
        ai_->noteUpload(u.texture, t.width, t.height, u, rec.pixels().data() + u.pixels,
                        u.slotCount ? rec.slots().data() + u.firstSlot : nullptr, u.slotCount);
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
    // 0.59: the colours over the world, before the interface's first draw (as Direct3D 12's applyGrade); 0.61: the
    // torches' light before them
    // 0.90: a pass of the driver's own draws over what the game drew so far: the gathered call goes first
    if (s.ui && worldDrawn_ && (!nightDone_ || !lightDone_ || !gradeDone_)) flushBatch();
    if (s.ui && worldDrawn_ && !nightDone_) applyNight();  // 0.63: the lights shine out of it
    if (s.ui && worldDrawn_ && !lightDone_) applyTorchLight();
    if (s.ui && worldDrawn_ && !gradeDone_) applyGrade();
    if (s.ui && worldDrawn_ && !flies_.empty()) applyFireflies();  // 1.0.1: with the torches' light
    if (s.ui && glowDrawn_) addGlow();  // 1.0: the world's glow onto it, before the interface
    // 1.0: from the world's first draw that adds light, its draws write into the glow too
    if (!s.ui && s.blend && s.dstBlend == d3d7::BLEND_ONE && !glowActive_ && glowWanted()) startGlow();
    const bool toGlow = !s.ui && glowActive_ && d.effect != uint8_t(effect_exe::Kind::Aura);  // 1.25
    if (!s.ui && d.vertexCount == 4 && !objectTextures_.empty() && torchesWanted()) noteTorch(rec, d);
    // 1.0.1: a firefly the game draws - drawn anew at the interface instead
    if (!s.ui && d.effect == uint8_t(effect_exe::Kind::Firefly) && firefliesWanted() && takeFirefly(rec, d)) return;
    if (!s.ui) worldDrawn_ = true;
    // 0.60.1: the characters' shadows, before the first blended draw after the ground (as draw.cpp does)
    if (!s.ui && d.fvf == d3d7::FVF_TL2) groundDrawn_ = true;
    if (!s.ui && s.blend && groundDrawn_ && !shadowsDone_ && shadowsWanted()) {
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
            ID3D11ShaderResourceView* copy = t[i] != &white_ ? ai_->copyOf(d.texture[i], s.ui) : nullptr;
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
    Constants c{};
    c.targetSize[0] = float(target_.width);
    c.targetSize[1] = float(target_.height);
    packStage(s.stage[0], d.keyed[0], c.stage0);
    packStage(s.stage[1], d.keyed[1], c.stage1);
    c.alphaTest = s.alphaTest ? uint32_t(s.alphaFunc) | uint32_t(s.alphaRef) << 8 | 1u << 16 : 0u;
    c.fogEnable = s.fog ? 1u : 0u;
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
    }
    float swayTop = 0.0f, swayBottom = 0.0f;  // 0.64: leaves in the wind (as draw.cpp)
    if (!s.ui && !twin && c.uiMode == 0 && effects_.foliage && effects_.foliageStrength > 0 && t[0] && t[0]->leafy &&
        d.fvf != d3d7::FVF_TL2 && yExtent(rec, d, swayTop, swayBottom)) {
        const unsigned long long now = GetTickCount64();
        if (!clockStart_) clockStart_ = now;
        c.twin[0] = 1.2f * float(effects_.foliageStrength) / 100.0f;
        c.twin[1] = swayBottom;
        c.twin[2] = swayBottom - swayTop;
        c.twin[3] = float(double(now - clockStart_) / 1000.0);
    }
    if (!s.ui && !twin && c.uiMode == 0 && t[0] && t[0]->leafy && d.fvf != d3d7::FVF_TL2) {  // 1.7: the trees
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
    if (water) {  // the game's camera, the waves' time and the page's settings (water_ps)
        std::memcpy(c.reproject, viewToWorld_, sizeof c.reproject);
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        c.sharpen = waterTime();
        c.footprint = float(effects_.waterWaves) / 100.0f;
        c.twin[0] = float(effects_.waterReflections) / 100.0f;
        c.twin[1] = float(effects_.waterClarity) / 100.0f;
        // 1.7: the water's life (water_ps waterLife): fish, a swamp's bubbles, the surf's swash
        const living::Place place = living::placeOf(map_, outdoors(map_), false);
        c.twin[3] = float((effects_.fish && !place.cave ? 1 : 0) + (effects_.swamp && place.swamp ? 2 : 0) +
                          (effects_.shoreFoam ? 4 : 0));
        if (!waterCells_++) LOG_INFO("water: the map's water found, drawn anew (Direct3D 11)");
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
    const bool soft = !water && !s.ui && effects_.softParticles && d.vertexCount == 4 && s.blend && s.zEnable &&
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
    if (materialsWanted() && !s.ui && !water && !soft && !adds && !twin && !d.twin && s.zWrite && t[0] != &white_ &&
        !t[0]->leafy && c.uiMode == 0 &&
        !(d.vertexCount == 4 &&
          ai::flatQuad(rec.vertexBytes().data() + d.vertexOffset, stride, d.vertexCount, d.indexCount))) {
        Texture& own = textures_.find(d.texture[0])->second;
        if (ensureMaterial(own, srvs[0])) mat = own.matSrv;
        bindTarget();  // a material made just now bound its own
    }
    if (mat) materialConstants(d.fvf == d3d7::FVF_TL2, c);
    // 0.90: the draw call's state; the draw goes on the gathered call when it is the same and its vertices follow
    Issue is;
    is.c = c;
    const int aniso = s.ui ? 0 : anisotropy_;
    is.samplers[0] = samplerState(sampled[0], aniso);
    is.samplers[1] = samplerState(sampled[1], aniso);
    is.blend = blendState(s.blend, d3dmap::blendOf(s.srcBlend, d3d7::BLEND_ONE),
                          d3dmap::blendOf(s.dstBlend, d3d7::BLEND_ZERO),
                          D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                              D3D11_COLOR_WRITE_ENABLE_BLUE);
    is.depth = depthState(s.zEnable, s.zWrite, d3dmap::compareOf(s.zFunc));
    is.raster = rasterState(d3dmap::cullOf(s.cull));
    is.srvs[0] = srvs[0];
    is.srvs[1] = srvs[1];
    is.srvs[2] = mat;
    is.layout = layout_[d.fvf == d3d7::FVF_TL2 ? 1 : 0];
    is.vs = water ? waterVs_ : drawVs_;
    is.ps = water ? (toGlow ? waterGlowPs_ : waterPs_) : toGlow ? drawGlowPs_ : drawPs_;
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
           layout == o.layout &&
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
    if (is.ps == waterPs_ && waterPs_) {  // 0.99: the picture and the depth under the water (t2, t3)
        ID3D11ShaderResourceView* under[2] = {scene_.srv, depthCopySrv_};
        ctx_->PSSetShaderResources(2, 2, under);
    } else if (is.srvs[2]) {  // 1.14: the material (t2), bound with each call that reads it
        ctx_->PSSetShaderResources(2, 1, &is.srvs[2]);
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
    if (scene_.width != target_.width || scene_.height != target_.height) {
        dropTarget(scene_);
        if (!makeTarget(scene_, target_.width, target_.height)) return false;
    }
    D3D11_TEXTURE2D_DESC dd{};
    depth_->GetDesc(&dd);
    D3D11_TEXTURE2D_DESC cd{};
    if (depthCopy_) depthCopy_->GetDesc(&cd);
    if (!depthCopy_ || cd.Width != dd.Width || cd.Height != dd.Height) {
        drop(depthCopySrv_);
        drop(depthCopyDsv_);
        drop(depthCopy_);
        if (!makeDepth(int(dd.Width), int(dd.Height), depthCopy_, depthCopyDsv_)) return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};
        v.Format = DXGI_FORMAT_R32_FLOAT;
        v.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        v.Texture2D.MipLevels = 1;
        if (FAILED(device_->CreateShaderResourceView(depthCopy_, &v, &depthCopySrv_))) {
            depthCopySrv_ = nullptr;
            return false;
        }
    }
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx_->PSSetShaderResources(2, 2, none);  // neither copy bound while it is written
    ctx_->CopyResource(scene_.texture, target_.texture);
    ctx_->CopyResource(depthCopy_, depth_);
    waterReady_ = true;
    return true;
}

float Renderer11::waterTime() {
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    if (!waterClock_.QuadPart) waterClock_ = now;
    return float(double(now.QuadPart - waterClock_.QuadPart) / double(freq.QuadPart));
}

void Renderer11::setCamera(bool valid, const float view[12], float) {
    float v4[16], inv[16];
    camera::affine4(view, v4);
    cameraValid_ = valid && camera::inverse4(v4, inv);
    if (cameraValid_)
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c) viewToWorld_[3 * r + c] = inv[4 * r + c];
}

bool Renderer11::depthViews() {
    if (!depth_) return false;
    if (depthSrvOf_ != depth_) {
        drop(depthSrv_);
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};
        v.Format = DXGI_FORMAT_R32_FLOAT;
        v.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        v.Texture2D.MipLevels = 1;
        depthSrvOf_ = depth_;
        if (FAILED(device_->CreateShaderResourceView(depth_, &v, &depthSrv_))) depthSrv_ = nullptr;
    }
    if (readOnlyDsvOf_ != depth_) {
        drop(readOnlyDsv_);
        D3D11_DEPTH_STENCIL_VIEW_DESC v{};
        v.Format = DXGI_FORMAT_D32_FLOAT;
        v.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        v.Flags = D3D11_DSV_READ_ONLY_DEPTH;
        readOnlyDsvOf_ = depth_;
        if (FAILED(device_->CreateDepthStencilView(depth_, &v, &readOnlyDsv_))) readOnlyDsv_ = nullptr;
    }
    return depthSrv_ && readOnlyDsv_;
}

void Renderer11::execute(const FrameRecord& record) {
    if (lost_ || !target_.texture || record.empty()) return;
    LARGE_INTEGER started;  // 0.89: the processor's time in drawing, for the log
    QueryPerformanceCounter(&started);
    timeBegin();
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
                d.BufferCount = 2;
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
    if (!target_.texture || !window || source.empty()) return;
    if (!ensureSwapChain(window, UINT(std::max(1, outputWidth)), UINT(std::max(1, outputHeight)),
                         options.allowTearing)) {
        Sleep(15);
        return;
    }
    timeBegin();
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ctx_->ClearRenderTargetView(backRtv_, black);
    // Fit the source into the window, keeping its shape, on whole pixels: bars where the shapes differ.
    const Rect fit = fitPicture(source.width(), source.height(), int(swapWidth_), int(swapHeight_));
    if (!(fit == picture_)) {
        picture_ = fit;
        LOG_DEBUG("picture %dx%d of the game shown as %dx%d at %d,%d in the %ux%u window", source.width(),
                  source.height(), fit.width(), fit.height(), fit.left, fit.top, swapWidth_, swapHeight_);
    }
    // The source is in game pixels: into target pixels by each axis' own ratio.
    const auto tx = [this](int x) { return float(double(x) * target_.width / gameWidth_); };
    const auto ty = [this](int y) { return float(double(y) * target_.height / gameHeight_); };
    const float from[4] = {tx(source.left), ty(source.top), tx(source.right), ty(source.bottom)};
    if (worldDrawn_ && !nightDone_) applyNight();  // 0.63
    if (worldDrawn_ && !lightDone_) applyTorchLight();  // 0.61
    if (!flies_.empty()) applyFireflies();  // 1.0.1
    if (worldDrawn_ && !gradeDone_) applyGrade();  // 0.59: a frame without an interface
    if (glowDrawn_) addGlow();  // 1.0: likewise
    blit(target_.srv, target_.width, target_.height, from, backRtv_, fit, int(swapWidth_), int(swapHeight_),
         options.sharpen);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    if (!options.screenshot.empty()) saveScreenshot(options.screenshot);  // 0.54
    gradeDone_ = worldDrawn_ = false;  // 0.59: the next frame
    groundDrawn_ = shadowsDone_ = false;  // 0.60.1
    lightDone_ = nightDone_ = false;  // 0.61, 0.63
    waterCopied_ = waterReady_ = false;  // 0.99
    materialsMade_ = 0;  // 1.14
    flames_.clear();
    timeEnd();
    LARGE_INTEGER ended;  // 0.89
    QueryPerformanceCounter(&ended);
    stats_.ticks += ended.QuadPart - started.QuadPart;
    ++stats_.frames;
    timeFrame();
    pacer_.wait(options.fpsLimit);  // 0.85: kept to the frame
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
    const bool night = effects_.night && effects_.nightNow > 0, day = effects_.dayCycle && effects_.dayNow >= 0;
    const region_look::Look look = region_look::of(map_, effects_.regionLooks ? effects_.regionLooksStrength : 0);
    if ((!night && !day && !look.region[0]) || !nightPs_ || !target_.texture) return;
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
void Renderer11::worldPass(ID3D11PixelShader* ps, const Constants& c) {
    if (scene_.width != target_.width || scene_.height != target_.height) {
        dropTarget(scene_);
        if (!makeTarget(scene_, target_.width, target_.height)) return;
    }
    ctx_->CopyResource(scene_.texture, target_.texture);
    targetBound_ = false;
    ID3D11ShaderResourceView* none[3] = {nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 3, none);
    ctx_->OMSetRenderTargets(1, &target_.rtv, nullptr);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(target_.width), float(target_.height), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    const D3D11_RECT all{0, 0, target_.width, target_.height};
    ctx_->RSSetScissorRects(1, &all);
    setConstants(c);
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO,
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
    ctx_->PSSetShaderResources(2, 1, &scene_.srv);  // sceneColor : register(t2)
    ctx_->Draw(3, 0);
    ctx_->PSSetShaderResources(0, 3, none);
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

void Renderer11::setAiShown(uint8_t kinds) {
    if (ai_) {
        ai_->shown = kinds != 0;
        ai_->kindsShown = kinds;
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
        static const char* const kPasses[kEffectTimes11] = {"actor_shadows", "lights", "night", "grade"};
        recent_.gpuMs = recentMs_ / recentFrames_;
        recent_.effects.clear();
        for (int e = 0; e < kEffectTimes11; ++e) {
            if (recentEffectMs_[e] > 0.0) recent_.effects[kPasses[e]] = recentEffectMs_[e] / recentFrames_;
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
                                                            "colour"};
        std::string effects;
        for (int e = 0; e < kEffectTimes11; ++e) {
            if (effectMs_[e] <= 0.0) continue;
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
