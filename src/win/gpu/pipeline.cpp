#include <windows.h>

#include <d3d12.h>
#include <d3dcompiler.h>

#include <cstring>

#include "core/d3d7.hpp"
#include "win/gpu/impl.hpp"
#include "win/gpu/shaders.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

using SerializeRootSignatureFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION,
                                                  ID3DBlob**, ID3DBlob**);

bool compile(pD3DCompile fn, const char* entry, const char* profile, ID3DBlob*& out, std::string& error) {
    const char* src = shaderSource();
    ID3DBlob* errors = nullptr;
    const HRESULT hr = fn(src, std::strlen(src), "uaro-graphics.hlsl", nullptr, nullptr, entry, profile,
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &out, &errors);
    if (FAILED(hr)) {
        error = std::string("shader ") + entry + ": " +
                (errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                        : std::string("compile failed"));
    }
    releaseCom(errors);
    return SUCCEEDED(hr);
}

// One D3D12 sampler from a D3D7 stage. D3D7 address modes and D3D12's share their numbers (WRAP 1 ... BORDER 4).
D3D12_SAMPLER_DESC samplerFor(const StageSetup& s) {
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
    d.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    d.BorderColor[0] = float((s.borderColor >> 16) & 255) / 255.0f;
    d.BorderColor[1] = float((s.borderColor >> 8) & 255) / 255.0f;
    d.BorderColor[2] = float(s.borderColor & 255) / 255.0f;
    d.BorderColor[3] = float(s.borderColor >> 24) / 255.0f;
    d.MinLOD = 0.0f;
    d.MaxLOD = mips ? D3D12_FLOAT32_MAX : 0.0f;
    return d;
}

uint64_t samplerKey(const StageSetup& s) {
    return uint64_t(s.addressU) | uint64_t(s.addressV) << 4 | uint64_t(s.magFilter) << 8 |
           uint64_t(s.minFilter) << 12 | uint64_t(s.mipFilter) << 16 | uint64_t(s.borderColor) << 20;
}

}  // namespace

bool Renderer::Impl::createPipelines(std::string& error) {
    compiler = LoadLibraryA("d3dcompiler_47.dll");
    const auto compileFn = compiler ? reinterpret_cast<pD3DCompile>(reinterpret_cast<void*>(GetProcAddress(compiler, "D3DCompile")))
                                    : nullptr;
    if (!compileFn) {
        error = "d3dcompiler_47.dll is missing";
        return false;
    }
    if (!compile(compileFn, "draw_vs", "vs_5_0", drawVs, error) || !compile(compileFn, "draw_ps", "ps_5_0", drawPs, error) ||
        !compile(compileFn, "present_vs", "vs_5_0", presentVs, error) ||
        !compile(compileFn, "present_ps", "ps_5_0", presentPs, error))
        return false;

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 2;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE samplerRange{};
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.NumDescriptors = 2;
    samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER params[3] = {};
    params[kRootConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootConstants].Constants.ShaderRegister = 0;
    params[kRootConstants].Constants.Num32BitValues = kConstantCount;
    params[kRootConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[kRootTextures].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootTextures].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootTextures].DescriptorTable.pDescriptorRanges = &srvRange;
    params[kRootTextures].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[kRootSamplers].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootSamplers].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootSamplers].DescriptorTable.pDescriptorRanges = &samplerRange;
    params[kRootSamplers].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 3;
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
    p.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&presentPipeline)))) {
        error = "present pipeline";
        return false;
    }
    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::samplerTable(const StageSetup& s0, const StageSetup& s1) {
    const uint64_t key = samplerKey(s0) * 1000003ull ^ samplerKey(s1);
    auto it = samplerPairs.find(key);
    if (it == samplerPairs.end()) {
        if (nextSampler + 2 > kSamplerCount) {  // full: start over once the GPU is done with the old pairs
            waitIdle();
            samplerPairs.clear();
            nextSampler = 0;
            begin();
        }
        const UINT index = nextSampler;
        nextSampler += 2;
        D3D12_CPU_DESCRIPTOR_HANDLE h = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(index) * samplerStep;
        const D3D12_SAMPLER_DESC d0 = samplerFor(s0);
        const D3D12_SAMPLER_DESC d1 = samplerFor(s1);
        device->CreateSampler(&d0, h);
        h.ptr += samplerStep;
        device->CreateSampler(&d1, h);
        it = samplerPairs.emplace(key, index).first;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE g = samplerHeap->GetGPUDescriptorHandleForHeapStart();
    g.ptr += UINT64(it->second) * samplerStep;
    return g;
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::textureTable(UINT srv0, UINT srv1) {
    if (ringSrv + 2 > kGpuSrvCount) ringSrv = 0;  // the ring is far larger than one submission ever needs
    D3D12_CPU_DESCRIPTOR_HANDLE dst = gpuSrv->GetCPUDescriptorHandleForHeapStart();
    dst.ptr += SIZE_T(ringSrv) * srvStep;
    device->CopyDescriptorsSimple(1, dst, cpuSrvHandle(srv0), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    dst.ptr += srvStep;
    device->CopyDescriptorsSimple(1, dst, cpuSrvHandle(srv1), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE g = gpuSrv->GetGPUDescriptorHandleForHeapStart();
    g.ptr += UINT64(ringSrv) * srvStep;
    ringSrv += 2;
    return g;
}

ID3D12PipelineState* Renderer::Impl::pipelineFor(const DrawSetup&, uint32_t) { return nullptr; }  // Task 8

}  // namespace uagfx::gpu
