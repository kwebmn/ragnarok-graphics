#include <windows.h>

#include <d3d12.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>

#include "core/d3d7.hpp"
#include "core/pipeline_key.hpp"
#include "core/shader_cache.hpp"
#include "win/gpu/impl.hpp"
#include "win/gpu/shaders.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::gpu {
namespace {

using SerializeRootSignatureFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION,
                                                  ID3DBlob**, ID3DBlob**);

// 0.39.4: of what the compiler said, the line of its error (a warning may come first), without the game's folder
// before the file's name; at most 200 letters.
std::string errorLine(const std::string& said) {
    std::string first;
    for (size_t start = 0; start < said.size();) {
        size_t end = said.find_first_of("\r\n", start);
        if (end == std::string::npos) end = said.size();
        std::string line = said.substr(start, end - start);
        const size_t file = line.find("uaro-graphics.hlsl");
        if (file != std::string::npos) line.erase(0, file);
        if (first.empty()) first = line;
        if (line.find("error") != std::string::npos) return line.substr(0, 200);
        start = end + 1;
    }
    return first.substr(0, 200);
}

// 0.39.5: the shaders' code kept from the last start (core/shader_cache.hpp) and this start's, written back when any
// of it had to be compiled.
struct ShaderStore {
    using CreateBlobFn = HRESULT(WINAPI*)(SIZE_T, ID3DBlob**);
    CreateBlobFn createBlob = nullptr;
    std::vector<shader_cache::Shader> kept;
    std::vector<shader_cache::Shader> made;
    int reused = 0;
    int compiled = 0;
    // 0.40: each one done is told (the start screen): loaded or compiled, how many of how many.
    Renderer::StartSink sink = nullptr;
    void* context = nullptr;
    int total = 0;
    void told(bool loaded) const {
        if (sink)
            sink(context, start::Progress{loaded ? start::Stage::ShadersLoad : start::Stage::ShadersBuild,
                                          reused + compiled, total});
    }
};

// Into a file of its own, then over the old one in one step: another client starting meanwhile reads the old one or
// the new one, never a part of one.
void keepShaders(const std::string& file, const std::vector<uint8_t>& bytes) {
    if (!win::writeFileAtomic(file, bytes.data(), bytes.size()))
        LOG_INFO("shaders: cannot keep them in %s - compiled again at the next start", file.c_str());
}

bool compile(ShaderStore& store, pD3DCompile fn, const char* entry, const char* profile, ID3DBlob*& out,
             std::string& error) {
    for (const shader_cache::Shader& s : store.kept) {
        bool safe = false;
        if (!shader_cache::matches(s, entry, profile, safe) || FAILED(store.createBlob(s.code.size(), &out))) continue;
        std::memcpy(out->GetBufferPointer(), s.code.data(), s.code.size());
        if (safe) noteSafeShader();
        store.made.push_back(s);
        ++store.reused;
        store.told(true);
        return true;
    }
    const char* src = shaderSource();
    // 0.39.2: the compiler's optimiser fails now and then with an internal error ("argument pulled into unrelated
    // predicate": ray_glow_ps on a player's machine) - then again with less of it.
    // 0.76: and at last without the newest features (UAGFX_SAFE in shaders.cpp) - a compiler that will not take them
    // leaves the picture as before them rather than none.
    static const UINT kLevels[4] = {D3DCOMPILE_OPTIMIZATION_LEVEL3, D3DCOMPILE_OPTIMIZATION_LEVEL1,
                                    D3DCOMPILE_SKIP_OPTIMIZATION, D3DCOMPILE_OPTIMIZATION_LEVEL1};
    static const D3D_SHADER_MACRO kSafe[2] = {{"UAGFX_SAFE", "1"}, {nullptr, nullptr}};
    HRESULT hr = E_FAIL;
    std::string said;    // what the compiler said the first time
    bool safe = false;   // 2.2.2: made on the last try, without the newest features
    for (int i = 0; i < 4 && FAILED(hr); ++i) {
        ID3DBlob* errors = nullptr;
        out = nullptr;
        hr = fn(src, std::strlen(src), "uaro-graphics.hlsl", i == 3 ? kSafe : nullptr, nullptr, entry, profile,
                kLevels[i], 0, &out, &errors);
        if (FAILED(hr) && i == 0) {
            said = errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                          : std::string("compile failed");
            error = std::string("shader ") + entry + ": " + said;
        }
        if (SUCCEEDED(hr) && i == 3) {
            LOG_ERROR("shader %s: compiled without the newest features (soft effects, ground relief) - %s", entry,
                      errorLine(said).c_str());
            safe = true;
            noteSafeShader();
        }
        else if (SUCCEEDED(hr) && i > 0)  // 0.39.3: a line of it is enough then
            LOG_INFO("shader %s: compiled with less optimisation - %s", entry, errorLine(said).c_str());
        releaseCom(errors);
    }
    if (FAILED(hr)) return false;
    const auto* code = static_cast<const uint8_t*>(out->GetBufferPointer());
    store.made.push_back({entry, shader_cache::profileOf(profile, safe),
                          std::vector<uint8_t>(code, code + out->GetBufferSize())});
    ++store.compiled;
    store.told(false);
    return true;
}

// One D3D12 sampler from a D3D7 stage. D3D7 address modes and D3D12's share their numbers (WRAP 1 ... BORDER 4).
// Stage 4: a world texture the client filters linearly is filtered anisotropically when aniso is 2-16. 2.2.4: bias
// shifts the mip level (the world under DLSS, upscaleMipBias).
D3D12_SAMPLER_DESC samplerFor(const StageSetup& s, int aniso, float bias) {
    D3D12_SAMPLER_DESC d{};
    const bool linearMin = s.minFilter == d3d7::TFN_LINEAR || s.minFilter == d3d7::TFN_ANISOTROPIC;
    const bool linearMag = s.magFilter == d3d7::TFG_LINEAR || s.magFilter == d3d7::TFG_ANISOTROPIC;
    const bool mips = s.mipFilter == d3d7::TFP_POINT || s.mipFilter == d3d7::TFP_LINEAR;
    const bool linearMip = s.mipFilter == d3d7::TFP_LINEAR;
    d.Filter = D3D12_FILTER((linearMin ? 0x10 : 0) | (linearMag ? 0x4 : 0) | (linearMip ? 0x1 : 0));
    auto address = [](uint8_t a) {
        return a >= d3d7::TADDRESS_WRAP && a <= d3d7::TADDRESS_BORDER ? D3D12_TEXTURE_ADDRESS_MODE(a)
                                                                       : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    };
    d.AddressU = address(s.addressU);
    d.AddressV = address(s.addressV);
    d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d.MaxAnisotropy = 1;
    if (aniso >= 2 && linearMin && linearMag) {  // mips, when the texture has them, are linear then
        d.Filter = D3D12_FILTER_ANISOTROPIC;
        d.MaxAnisotropy = UINT(std::min(aniso, 16));
    }
    d.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    d.BorderColor[0] = float((s.borderColor >> 16) & 255) / 255.0f;
    d.BorderColor[1] = float((s.borderColor >> 8) & 255) / 255.0f;
    d.BorderColor[2] = float(s.borderColor & 255) / 255.0f;
    d.BorderColor[3] = float(s.borderColor >> 24) / 255.0f;
    d.MinLOD = 0.0f;
    d.MaxLOD = mips ? D3D12_FLOAT32_MAX : 0.0f;
    d.MipLODBias = bias;
    return d;
}

uint64_t samplerKey(const StageSetup& s, int aniso, float bias) {
    const uint64_t lower = uint64_t(std::clamp(int(std::lround(-bias * 16.0f)), 0, 127));  // sixteenths, 0 to -7.9
    return uint64_t(s.addressU) | uint64_t(s.addressV) << 4 | uint64_t(s.magFilter) << 8 |
           uint64_t(s.minFilter) << 12 | uint64_t(s.mipFilter) << 16 | uint64_t(s.borderColor) << 20 |
           uint64_t(aniso & 31) << 52 | lower << 57;
}

std::atomic<int> g_safeShaders{0};

}  // namespace

int safeShaderCount() { return g_safeShaders.load(); }
void noteSafeShader() { ++g_safeShaders; }

bool Renderer::Impl::createPipelines(std::string& error) {
    compiler = LoadLibraryA("d3dcompiler_47.dll");
    const auto compileFn = compiler ? reinterpret_cast<pD3DCompile>(
                                          reinterpret_cast<void*>(GetProcAddress(compiler, "D3DCompile")))
                                    : nullptr;
    if (!compileFn) {
        error = "d3dcompiler_47.dll is missing";
        return false;
    }
    // 0.39.5: the code the last start compiled from this source, if it is kept.
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    ShaderStore store;
    store.createBlob = reinterpret_cast<ShaderStore::CreateBlobFn>(
        reinterpret_cast<void*>(GetProcAddress(compiler, "D3DCreateBlob")));
    const uint64_t key = shader_cache::keyOf(shaderSource());
    std::string kept;
    if (store.createBlob && !shaderCacheFile.empty() && win::readTextFile(shaderCacheFile, kept))
        store.kept = shader_cache::unpack(key, std::vector<uint8_t>(kept.begin(), kept.end()));
    // 0.18, 0.19: the effects' shaders - one that does not compile leaves its effect off, the rest draws on.
    const struct {
        const char* entry;
        const char* profile;
        ID3DBlob** blob;
    } effects[] = {{"draw_glow_ps", "ps_5_0", &drawGlowPs},  {"glow_down_ps", "ps_5_0", &glowDownPs},
                   {"glow_up_ps", "ps_5_0", &glowUpPs},      {"glow_add_ps", "ps_5_0", &glowAddPs},
                   {"water_vs", "vs_5_0", &waterVs},         {"water_ps", "ps_5_0", &waterPs},
                   {"water_glow_ps", "ps_5_0", &waterGlowPs}, {"depth_copy_ps", "ps_5_0", &depthCopyPs},
                   {"depth_copy_ms_ps", "ps_5_0", &depthCopyMsPs}, {"ao_ps", "ps_5_0", &aoPs},
                   {"ao_blur_ps", "ps_5_0", &aoBlurPs},          {"ao_apply_ps", "ps_5_0", &aoApplyPs},
                   {"grade_ps", "ps_5_0", &gradePs},              {"dof_down_ps", "ps_5_0", &dofDownPs},
                   {"dof_blur_ps", "ps_5_0", &dofBlurPs},          {"dof_apply_ps", "ps_5_0", &dofApplyPs},
                   {"dof_focus_ps", "ps_5_0", &dofFocusPs},       {"mask_ps", "ps_5_0", &maskPs},
                   {"torch_light_vs", "vs_5_0", &torchLightVs},   {"torch_accum_ps", "ps_5_0", &torchAccumPs},
                   {"torch_apply_ps", "ps_5_0", &torchApplyPs},   {"torch_fire_ps", "ps_5_0", &torchFirePs},
                   {"smoke_ps", "ps_5_0", &smokePs},              {"smoke_glow_ps", "ps_5_0", &smokeGlowPs},
                   {"firefly_ps", "ps_5_0", &fireflyPs},          {"dust_ps", "ps_5_0", &dustPs},
                   {"ray_ps", "ps_5_0", &rayPs},                  {"ray_glow_ps", "ps_5_0", &rayGlowPs},
                   {"lava_ps", "ps_5_0", &lavaPs},                {"lava_glow_ps", "ps_5_0", &lavaGlowPs},
                   {"shadow_vs", "vs_5_0", &shadowVs},            {"shadow_ps", "ps_5_0", &shadowPs},
                   {"shadow_blur_ps", "ps_5_0", &shadowBlurPs},   {"shadow_apply_ps", "ps_5_0", &shadowApplyPs},
                   {"clouds_ps", "ps_5_0", &cloudsPs},            {"haze_ps", "ps_5_0", &hazePs},
                   {"heat_ps", "ps_5_0", &heatPs},                {"weather_ps", "ps_5_0", &weatherPs},
                   {"clouds_apply_ps", "ps_5_0", &cloudsApplyPs}, {"night_ps", "ps_5_0", &nightPs},
                   {"splash_ps", "ps_5_0", &splashPs},            {"waterfall_ps", "ps_5_0", &waterfallPs},
                   {"sky_ps", "ps_5_0", &skyPs},                  {"floor_reflect_ps", "ps_5_0", &floorReflectPs},
                   {"shafts_ps", "ps_5_0", &shaftsPs},            {"taa_ps", "ps_5_0", &taaPs},
                   {"bloom_down_ps", "ps_5_0", &bloomDownPs},
                   {"bloom_apply_ps", "ps_5_0", &bloomApplyPs},   {"steps_ps", "ps_5_0", &stepsPs},
                   {"grass_ps", "ps_5_0", &grassPs},              {"embers_ps", "ps_5_0", &embersPs},
                   {"half_over_ps", "ps_5_0", &halfOverPs},       {"flare_ps", "ps_5_0", &flarePs},
                   {"ground_life_ps", "ps_5_0", &groundLifePs},   {"critters_ps", "ps_5_0", &crittersPs},
                   {"critters_vs", "vs_5_0", &crittersVs},        {"material_ps", "ps_5_0", &materialPs},
                   {"sky_light_ps", "ps_5_0", &skyLightPs},
                   {"fog_light_ps", "ps_5_0", &fogLightPs},      {"mirror_vs", "vs_5_0", &mirrorVs},
                   {"mirror_ps", "ps_5_0", &mirrorPs}};
    store.sink = startSink;  // 0.40
    store.context = startContext;
    store.total = 6 + int(sizeof effects / sizeof effects[0]);  // the six below and the effects'
    if (!compile(store, compileFn, "draw_vs", "vs_5_0", drawVs, error) ||
        !compile(store, compileFn, "draw_ps", "ps_5_0", drawPs, error) ||
        !compile(store, compileFn, "present_vs", "vs_5_0", presentVs, error) ||
        !compile(store, compileFn, "present_ps", "ps_5_0", presentPs, error) ||
        !compile(store, compileFn, "motion_ps", "ps_5_0", motionPs, error) ||
        !compile(store, compileFn, "copy_ps", "ps_5_0", copyPs, error))
        return false;
    for (const auto& e : effects) {
        std::string why;
        if (!compile(store, compileFn, e.entry, e.profile, *e.blob, why)) {
            *e.blob = nullptr;
            LOG_ERROR("%s: its effect stays off", why.c_str());
        }
    }
    QueryPerformanceCounter(&t1);
    LOG_INFO("shaders: %d kept from the last start, %d compiled, in %.0f ms", store.reused, store.compiled,
             double(t1.QuadPart - t0.QuadPart) * 1000.0 / double(freq.QuadPart));
    if (store.compiled > 0 && !shaderCacheFile.empty())
        keepShaders(shaderCacheFile, shader_cache::pack(key, store.made));

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 2;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE samplerRange{};
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.NumDescriptors = 2;
    samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE sceneRange{};  // 0.19: the water's t2-t4
    sceneRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    sceneRange.NumDescriptors = 4;  // 1.16: and t5, the world mirrored under the water
    sceneRange.BaseShaderRegister = 2;
    sceneRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER params[4] = {};
    params[kRootConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootConstants].Constants.ShaderRegister = 0;
    params[kRootConstants].Constants.Num32BitValues = kConstantCount;
    params[kRootConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[kRootTextures].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootTextures].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootTextures].DescriptorTable.pDescriptorRanges = &srvRange;
    // 1.7: the vertex shaders too - critters_vs reads its sprites' list (t1)
    params[kRootTextures].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[kRootSamplers].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootSamplers].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootSamplers].DescriptorTable.pDescriptorRanges = &samplerRange;
    params[kRootSamplers].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[kRootScene].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootScene].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootScene].DescriptorTable.pDescriptorRanges = &sceneRange;
    params[kRootScene].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 4;
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    const auto serialize = reinterpret_cast<SerializeRootSignatureFn>(
        reinterpret_cast<void*>(GetProcAddress(d3d12, "D3D12SerializeRootSignature")));
    ID3DBlob* blob = nullptr;
    ID3DBlob* errors = nullptr;
    if (!serialize || FAILED(serialize(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)) ||
        FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                           __uuidof(ID3D12RootSignature), reinterpret_cast<void**>(&root)))) {
        error = "root signature";
        releaseCom(blob);
        releaseCom(errors);
        return false;
    }
    releaseCom(blob);
    releaseCom(errors);

    if (!presentPipelineFor(1)) {
        error = "present pipeline";
        return false;
    }
    return true;
}

bool Renderer::Impl::createUpscalePipelines() {
    if (motionPipeline && overPipeline && copyPipeline) return true;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {presentVs->GetBufferPointer(), presentVs->GetBufferSize()};
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.SampleDesc.Count = 1;
    p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (!motionPipeline) {
        p.PS = {motionPs->GetBufferPointer(), motionPs->GetBufferSize()};
        p.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&motionPipeline)))) {
            LOG_ERROR("the motion pipeline failed");
            motionPipeline = nullptr;
            return false;
        }
    }
    if (!copyPipeline) {  // as it is: frame generation's interface
        p.PS = {copyPs->GetBufferPointer(), copyPs->GetBufferSize()};
        p.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&copyPipeline)))) {
            LOG_ERROR("the copy pipeline failed");
            copyPipeline = nullptr;
            return false;
        }
    }
    if (!overPipeline) {
        p.PS = {copyPs->GetBufferPointer(), copyPs->GetBufferSize()};  // the layer's own alpha, not 1
        p.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
        b.BlendEnable = TRUE;  // the layer is premultiplied: colour + picture * (1 - coverage)
        b.SrcBlend = D3D12_BLEND_ONE;
        b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOp = D3D12_BLEND_OP_ADD;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;
        b.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        b.LogicOp = D3D12_LOGIC_OP_NOOP;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&overPipeline)))) {
            LOG_ERROR("the layer pipeline failed");
            overPipeline = nullptr;
            return false;
        }
    }
    return true;
}

// 0.65: the picture onto the HDR10 back buffer (present_ps, toPq).
ID3D12PipelineState* Renderer::Impl::hdrPipeline() {
    if (presentHdrPipeline) return presentHdrPipeline;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {presentVs->GetBufferPointer(), presentVs->GetBufferSize()};
    p.PS = {presentPs->GetBufferPointer(), presentPs->GetBufferSize()};
    p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
    p.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&presentHdrPipeline)))) {
        LOG_ERROR("HDR: the present pipeline failed");
        presentHdrPipeline = nullptr;
    }
    return presentHdrPipeline;
}

ID3D12PipelineState* Renderer::Impl::presentPipelineFor(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (presentPipelines[code]) return presentPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {presentVs->GetBufferPointer(), presentVs->GetBufferSize()};
    p.PS = {presentPs->GetBufferPointer(), presentPs->GetBufferSize()};
    p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.SampleDesc.Count = 1u << code;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&presentPipelines[code])))) {
        LOG_ERROR("present pipeline for %u samples failed", 1u << code);
        presentPipelines[code] = nullptr;
    }
    return presentPipelines[code];
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::samplerTable(const StageSetup& s0, const StageSetup& s1, int aniso,
                                                          float bias) {
    const KeyPair key{samplerKey(s0, aniso, bias), samplerKey(s1, aniso, bias)};
    auto it = samplerPairs.find(key);
    if (it == samplerPairs.end()) {
        if (nextSampler + 2 > kSamplerCount) return samplerHeap->GetGPUDescriptorHandleForHeapStart();  // never: makeRoom()
        const UINT index = nextSampler;
        nextSampler += 2;
        D3D12_CPU_DESCRIPTOR_HANDLE h = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(index) * samplerStep;
        const D3D12_SAMPLER_DESC d0 = samplerFor(s0, aniso, bias);
        const D3D12_SAMPLER_DESC d1 = samplerFor(s1, aniso, bias);
        device->CreateSampler(&d0, h);
        h.ptr += samplerStep;
        device->CreateSampler(&d1, h);
        it = samplerPairs.emplace(key, index).first;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE g = samplerHeap->GetGPUDescriptorHandleForHeapStart();
    g.ptr += UINT64(it->second) * samplerStep;
    return g;
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::sceneTable(UINT t2, UINT t3, UINT t4, UINT t5) {
    if (ringSrv + 4 > kRingPerSlot) ringSrv = kRingPerSlot - 4;  // never: makeRoom() ran first; stay in this part
    const UINT index = slot * kRingPerSlot + ringSrv;
    ringSrv += 4;
    D3D12_CPU_DESCRIPTOR_HANDLE dst = gpuSrv->GetCPUDescriptorHandleForHeapStart();
    dst.ptr += SIZE_T(index) * srvStep;
    for (const UINT srv : {t2, t3, t4, t5 == kNoSrv ? t2 : t5}) {
        device->CopyDescriptorsSimple(1, dst, cpuSrvHandle(srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        dst.ptr += srvStep;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE g = gpuSrv->GetGPUDescriptorHandleForHeapStart();
    g.ptr += UINT64(index) * srvStep;
    return g;
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::textureTable(UINT srv0, UINT srv1) {
    if (ringSrv + 2 > kRingPerSlot) ringSrv = kRingPerSlot - 2;  // never: makeRoom() ran first; stay in this part
    const UINT index = slot * kRingPerSlot + ringSrv;
    ringSrv += 2;
    D3D12_CPU_DESCRIPTOR_HANDLE dst = gpuSrv->GetCPUDescriptorHandleForHeapStart();
    dst.ptr += SIZE_T(index) * srvStep;
    device->CopyDescriptorsSimple(1, dst, cpuSrvHandle(srv0), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    dst.ptr += srvStep;
    device->CopyDescriptorsSimple(1, dst, cpuSrvHandle(srv1), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE g = gpuSrv->GetGPUDescriptorHandleForHeapStart();
    g.ptr += UINT64(index) * srvStep;
    return g;
}

// Room for one more draw or copy, made before it records anything (1B review M4, M5). A new sampler pair needs a
// free place in the cache - when it is full, every list using the old pairs has to finish first - and the two texture
// descriptors come from this list's part of the ring. Either way a new list starts here, never in the middle of a
// draw. False when the GPU is lost.
bool Renderer::Impl::makeRoom(const StageSetup& s0, const StageSetup& s1, int aniso, UINT descriptors, float bias) {
    if (nextSampler + 2 > kSamplerCount &&
        samplerPairs.find(KeyPair{samplerKey(s0, aniso, bias), samplerKey(s1, aniso, bias)}) == samplerPairs.end()) {
        waitIdle();
        samplerPairs.clear();
        nextSampler = 0;
        if (!begin()) return false;
    }
    // 1.16: a scene table holds four now - room for one more than the callers count
    if (ringSrv + descriptors + 2 > kRingPerSlot) {
        submit();
        if (!begin()) return false;
    }
    return open;
}

// D3D7 and D3D12 share the numbers of blend factors (ZERO 1 ... SRCALPHASAT 11), comparison functions
// (NEVER 1 ... ALWAYS 8) and cull modes (NONE 1, CW = FRONT 2, CCW = BACK 3 with clockwise front faces), and both
// count winding in window space (y down), so the states map one to one.
// Stage 4: the target's samples, and alpha to coverage when the draw asks for it (coverage). 0.18: toGlow, the
// glow target as the second one.
ID3D12PipelineState* Renderer::Impl::pipelineFor(const DrawSetup& s, uint32_t fvf, bool coverage, bool toLayer,
                                                 bool toGlow, bool water, bool mask, DrawShader shader) {
    PipelineKey k = pipelineKey(s, fvf);
    k.samples = uint8_t(toLayer ? 1 : samples);
    k.alphaToCoverage = coverage && !toLayer;
    k.layer = toLayer;
    k.glow = toGlow && !toLayer;
    k.water = water && !toLayer;
    k.mask = mask && !toLayer;
    k.shader = toLayer ? DrawShader::Game : shader;
    return pipelineForKey(packPipelineKey(k));
}

ID3D12PipelineState* Renderer::Impl::pipelineForKey(uint64_t key) {
    auto it = pipelines.find(key);
    if (it != pipelines.end()) return it->second;
    const PipelineKey k = unpackPipelineKey(key);

    // FVF 0x1C4 has one UV set: TEXCOORD1 reads the same bytes as TEXCOORD0.
    const D3D12_INPUT_ELEMENT_DESC layout[5] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, k.twoUv ? 32u : 24u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.BlendState.AlphaToCoverageEnable = k.alphaToCoverage ? TRUE : FALSE;
    p.VS = {drawVs->GetBufferPointer(), drawVs->GetBufferSize()};
    p.PS = {drawPs->GetBufferPointer(), drawPs->GetBufferSize()};
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = k.blend ? TRUE : FALSE;
    b.SrcBlend = D3D12_BLEND(k.srcBlend ? uint32_t(k.srcBlend) : uint32_t(d3d7::BLEND_ONE));
    b.DestBlend = D3D12_BLEND(k.dstBlend ? uint32_t(k.dstBlend) : uint32_t(d3d7::BLEND_ZERO));
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ONE;  // the target's alpha is never read (DESTALPHA reads 1): keep it simple
    b.DestBlendAlpha = D3D12_BLEND_ZERO;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = k.cull >= d3d7::CULL_NONE && k.cull <= d3d7::CULL_CCW ? D3D12_CULL_MODE(k.cull)
                                                                                        : D3D12_CULL_MODE_NONE;
    p.RasterizerState.FrontCounterClockwise = FALSE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.DepthStencilState.DepthEnable = k.zEnable ? TRUE : FALSE;
    p.DepthStencilState.DepthWriteMask = k.zWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    p.DepthStencilState.DepthFunc = k.zFunc >= d3d7::CMP_NEVER && k.zFunc <= d3d7::CMP_ALWAYS
                                        ? D3D12_COMPARISON_FUNC(k.zFunc)
                                        : D3D12_COMPARISON_FUNC_ALWAYS;
    p.InputLayout = {layout, 5};
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    p.SampleDesc.Count = k.samples;
    // 0.18, 0.19: an effect whose shaders did not compile (a key saved by another run) builds nothing.
    if ((k.glow && !drawGlowPs) || (k.water && (!waterVs || !waterPs || !waterGlowPs)) ||
        (k.shader == DrawShader::Smoke && (!smokePs || !smokeGlowPs)) ||
        (k.shader == DrawShader::Ray && (!rayPs || !rayGlowPs)) ||
        (k.shader == DrawShader::Lava && (!lavaPs || !lavaGlowPs))) {
        pipelines.emplace(key, nullptr);
        return nullptr;
    }
    if (k.glow) {  // 0.18: the light it adds into the glow target too, blended alike (one blend for both)
        p.PS = {drawGlowPs->GetBufferPointer(), drawGlowPs->GetBufferSize()};
        p.NumRenderTargets = 2;
        p.RTVFormats[1] = DXGI_FORMAT_R11G11B10_FLOAT;
    }
    if (k.mask) {  // 0.22.3: its coverage into the world's alpha too, gathered (DLSS's mask, composite())
        b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;
        b.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    }
    if (k.shader == DrawShader::Smoke) {  // 0.24: a puff of the chimneys' smoke, blended as the game blends it
        ID3DBlob* ps = k.glow ? smokeGlowPs : smokePs;
        p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    }
    if (k.shader == DrawShader::Ray) {  // 0.30: a beam of the forest's light, blended as the game blends it
        ID3DBlob* ps = k.glow ? rayGlowPs : rayPs;
        p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    }
    if (k.shader == DrawShader::Lava) {  // 0.31: a cell of lava, its glow into the glow target
        ID3DBlob* ps = k.glow ? lavaGlowPs : lavaPs;
        p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    }
    if (k.water) {  // 0.19: a cell of water by the modern water's shaders
        p.VS = {waterVs->GetBufferPointer(), waterVs->GetBufferSize()};
        ID3DBlob* ps = k.glow ? waterGlowPs : waterPs;
        p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    }
    if (k.layer) {  // part 3 (DLSS): the interface layer keeps its coverage in alpha, premultiplied, for the composite
        b.BlendEnable = TRUE;
        b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        if (!k.blend) {  // opaque: the shader writes alpha 1 (a blend factor only scales the shader's alpha)
            b.SrcBlend = D3D12_BLEND_ONE;
            b.DestBlend = D3D12_BLEND_ZERO;
            b.SrcBlendAlpha = D3D12_BLEND_BLEND_FACTOR;
            b.DestBlendAlpha = D3D12_BLEND_ZERO;
        } else if (k.dstBlend == d3d7::BLEND_INVSRCALPHA) {  // over: the coverage adds up
            b.SrcBlendAlpha = D3D12_BLEND_ONE;
            b.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        } else {  // added light and the like: the coverage stays as it was
            b.SrcBlendAlpha = D3D12_BLEND_ZERO;
            b.DestBlendAlpha = D3D12_BLEND_ONE;
        }
        p.DepthStencilState.DepthEnable = FALSE;
        p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        p.DSVFormat = DXGI_FORMAT_UNKNOWN;
    }
    ID3D12PipelineState* pso = nullptr;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState), reinterpret_cast<void**>(&pso)))) {
        LOG_ERROR("pipeline for key 0x%llx failed", static_cast<unsigned long long>(key));
        pso = nullptr;
    }
    pipelines.emplace(key, pso);  // a failure is remembered too: no retry every draw
    if (pso && !pipelineFile.empty()) {  // built at the next start (1B review M8)
        std::ofstream f(pipelineFile, std::ios::binary | std::ios::app);
        f << pipelineKeyLine(key);
        f.close();
        if (!f) {
            LOG_INFO("cannot add to %s: pipelines are not remembered any more", pipelineFile.c_str());
            pipelineFile.clear();
        }
    }
    return pso;
}

}  // namespace uagfx::gpu
