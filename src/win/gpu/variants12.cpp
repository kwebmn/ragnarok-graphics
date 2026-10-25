#include <d3dcompiler.h>

#include <cstring>

#include "win/gpu/impl.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

// 3.0.5-beta.3 (R-T4BZ6Q, R-8Q9FJP - a Radeon 610M: the login's picture 0.85 ms by Direct3D 11's draw shaders of each
// state, 4.6 ms by the one for every state): the draw shader made for each state of the game's draws
// (core/shader_variants.hpp, Direct3D 11's since 2.5.16: variants11.cpp) for Direct3D 12 too - compiled with FXC and
// made into its pipeline on a thread of its own as the states come, the pipeline for every state drawing meanwhile;
// the code kept in the shader cache with the rest (the pipelines are made again at the next start, quickly).
namespace uagfx::gpu {
namespace {

constexpr size_t kMostVariants = 256;  // pipelines: some dozens of states, each in a few targets and blends

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

void Renderer::Impl::startVariants(uint64_t key, std::vector<shader_cache::Shader> made,
                                   const std::vector<shader_cache::Shader>& kept) {
    cacheKey = key;
    unsigned loaded = 0;
    for (const shader_cache::Shader& s : kept) {
        shader_variants::Key k;
        if (s.profile != "ps_5_0" || !shader_variants::parseName(s.entry, k) || variantCode.count(k)) continue;
        variantCode.emplace(k, s.code);
        made.push_back(s);
        ++loaded;
    }
    cacheShaders = std::move(made);
    InitializeCriticalSection(&variantLock);
    variantLockMade = true;
    variantsUsable = true;
    if (loaded) LOG_INFO("Direct3D 12: %u draw shaders of the game's own states kept from the last start", loaded);
}

ID3D12PipelineState* Renderer::Impl::variantPipeline(uint64_t pipelineKey, const Constants& c) {
    if (!variantsUsable || !variantsWanted) return nullptr;
    const PipelineKey p = unpackPipelineKey(pipelineKey);
    if (p.shader != DrawShader::Game || p.water) return nullptr;
    // its base as pipelineForKey chose: the lean one where the key asks and it compiled, the glow's in two targets
    const bool lean = p.lean && drawLeanPs && (!p.glow || drawGlowLeanPs);
    const shader_variants::Base base = p.glow ? (lean ? shader_variants::Base::GlowLean : shader_variants::Base::Glow)
                                              : (lean ? shader_variants::Base::Lean : shader_variants::Base::Full);
    VariantPso which;
    which.pipeline = pipelineKey;
    which.key = shader_variants::keyOf(base, c.stage0, c.stage1, c.alphaTest, c.fogEnable, c.colorKey, c.uiMode);
    const auto it = variantPsos.find(which);
    if (it != variantPsos.end()) return it->second;
    if (variantPsos.size() >= kMostVariants) return nullptr;
    variantPsos.emplace(which, nullptr);  // asked: once
    if (!variantThread) {  // the first: its thread
        variantWake = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        variantThread = variantWake ? CreateThread(nullptr, 0, &Renderer::Impl::variantMain, this, 0, nullptr)
                                    : nullptr;
        if (!variantThread) {
            variantsUsable = false;
            LOG_ERROR("Direct3D 12: no thread for the draw shaders of the game's states (error %lu)", GetLastError());
            return nullptr;
        }
    }
    EnterCriticalSection(&variantLock);
    variantJobs.push_back(which);
    LeaveCriticalSection(&variantLock);
    SetEvent(variantWake);
    return nullptr;
}

void Renderer::Impl::collectVariants() {
    if (!variantLockMade) return;
    EnterCriticalSection(&variantLock);
    std::vector<VariantMade> done;
    done.swap(variantDone);
    const bool idle = variantJobs.empty();
    LeaveCriticalSection(&variantLock);
    for (const VariantMade& d : done) {
        variantPsos[d.which] = d.pso;
        if (d.pso) ++variantsMade;
    }
    if (variantsMade != variantsLogged && !done.empty() && idle) {
        LOG_INFO("Direct3D 12: %u pipelines with the draw shaders of the game's own states", variantsMade);
        variantsLogged = variantsMade;
    }
}

void Renderer::Impl::stopVariants() {
    if (variantThread) {
        EnterCriticalSection(&variantLock);
        variantStop = true;
        LeaveCriticalSection(&variantLock);
        SetEvent(variantWake);
        WaitForSingleObject(variantThread, INFINITE);
        CloseHandle(variantThread);
        variantThread = nullptr;
    }
    if (variantWake) CloseHandle(variantWake);
    variantWake = nullptr;
    for (auto& v : variantPsos) drop(v.second);
    variantPsos.clear();
    for (VariantMade& d : variantDone) drop(d.pso);
    variantDone.clear();
    if (variantLockMade) DeleteCriticalSection(&variantLock);
    variantLockMade = false;
    variantsUsable = false;
}

DWORD WINAPI Renderer::Impl::variantMain(void* self) {
    static_cast<Renderer::Impl*>(self)->variantLoop();
    return 0;
}

void Renderer::Impl::variantLoop() {
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    if (!compiler) compiler = LoadLibraryA("d3dcompiler_43.dll");
    const auto compile =
        compiler ? reinterpret_cast<pD3DCompile>(reinterpret_cast<void*>(GetProcAddress(compiler, "D3DCompile")))
                 : nullptr;
    const char* source = shaderSource();
    for (;;) {
        WaitForSingleObject(variantWake, INFINITE);
        for (;;) {
            VariantPso which;
            bool have = false;
            EnterCriticalSection(&variantLock);
            const bool stop = variantStop;
            if (!stop && !variantJobs.empty()) {
                which = variantJobs.front();
                variantJobs.erase(variantJobs.begin());
                have = true;
            }
            LeaveCriticalSection(&variantLock);
            if (stop) {
                if (compiler) FreeLibrary(compiler);
                return;
            }
            if (!have) break;
            // its code: kept from the last start, made by an earlier pipeline of this state, or compiled now
            std::vector<uint8_t> code;
            bool fresh = false;
            const auto known = variantCode.find(which.key);  // this thread's alone after the start
            if (known != variantCode.end()) {
                code = known->second;
            } else if (compile) {
                const std::vector<shader_variants::Macro> m = shader_variants::macrosOf(which.key);
                std::vector<D3D_SHADER_MACRO> macros;
                for (const shader_variants::Macro& x : m) macros.push_back({x.name.c_str(), x.value.c_str()});
                macros.push_back({nullptr, nullptr});
                for (const UINT level : {D3DCOMPILE_OPTIMIZATION_LEVEL3, D3DCOMPILE_OPTIMIZATION_LEVEL1}) {
                    ID3DBlob* blob = nullptr;
                    ID3DBlob* errors = nullptr;
                    const HRESULT hr = compile(source, std::strlen(source), "uaro-graphics.hlsl", macros.data(),
                                               nullptr, shader_variants::entryOf(which.key), "ps_5_0", level, 0,
                                               &blob, &errors);
                    if (SUCCEEDED(hr) && blob) {
                        const auto* p = static_cast<const uint8_t*>(blob->GetBufferPointer());
                        code.assign(p, p + blob->GetBufferSize());
                    }
                    drop(blob);
                    drop(errors);
                    if (!code.empty()) break;
                }
                if (!code.empty()) {
                    variantCode.emplace(which.key, code);
                    fresh = true;
                }
            }
            VariantMade d;
            d.which = which;
            if (!code.empty()) {  // the device is free-threaded: made here, used by the drawing thread
                D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
                D3D12_INPUT_ELEMENT_DESC layout[5];
                const D3D12_SHADER_BYTECODE ps{code.data(), code.size()};
                if (describePipeline(unpackPipelineKey(which.pipeline), p, layout, &ps) &&
                    FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                               reinterpret_cast<void**>(&d.pso))))
                    d.pso = nullptr;
            }
            std::vector<uint8_t> file;
            EnterCriticalSection(&variantLock);
            if (fresh) cacheShaders.push_back({shader_variants::nameOf(which.key), "ps_5_0", code});
            variantDone.push_back(d);
            if (fresh && variantJobs.empty() && !shaderCacheFile.empty())
                file = shader_cache::pack(cacheKey, cacheShaders);
            LeaveCriticalSection(&variantLock);
            if (!file.empty()) win::writeFileAtomic(shaderCacheFile, file.data(), file.size());  // the next start's
        }
    }
}

}  // namespace uagfx::gpu
