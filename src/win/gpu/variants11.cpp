#include "win/gpu/renderer11.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstring>

#include "win/log.hpp"
#include "win/paths.hpp"

// 2.5.16: the draw shader made for each state of the game's draws (core/shader_variants.hpp) - Direct3D 7's stages,
// tests and fog as constants, the branches among them folded away by the compiler. Made with FXC on a thread of their
// own as the states come (the draw shader for every state draws meanwhile), kept in the shader cache with the rest.
namespace uagfx::gpu {
namespace {

constexpr size_t kMostVariants = 192;  // a game uses some dozens; more is a state that keeps changing

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

void Renderer11::startVariants(const std::string& cacheFile, uint64_t key, const char* profile,
                               std::vector<shader_cache::Shader> shaders,
                               const std::vector<shader_cache::Shader>& kept) {
    variantCacheFile_ = cacheFile;
    variantCacheKey_ = key;
    variantProfile_ = profile;
    unsigned loaded = 0;
    for (const shader_cache::Shader& s : kept) {
        shader_variants::Key k;
        if (s.profile != profile || !shader_variants::parseName(s.entry, k) || variants_.count(k) ||
            variants_.size() >= kMostVariants)
            continue;
        ID3D11PixelShader* ps = nullptr;
        if (FAILED(device_->CreatePixelShader(s.code.data(), s.code.size(), nullptr, &ps))) continue;
        variants_.emplace(k, ps);
        shaders.push_back(s);
        ++loaded;
    }
    cacheShaders_ = std::move(shaders);
    InitializeCriticalSection(&variantLock_);
    variantLockMade_ = true;
    variantsUsable_ = true;
    variantsMade_ = variantsLogged_ = loaded;
    if (loaded) LOG_INFO("Direct3D 11: %u draw shaders of the game's own states kept from the last start", loaded);
}

ID3D11PixelShader* Renderer11::variantOf(shader_variants::Base base, const Constants& c) {
    if (!variantsUsable_ || !variantsWanted_) return nullptr;
    const shader_variants::Key k =
        shader_variants::keyOf(base, c.stage0, c.stage1, c.alphaTest, c.fogEnable, c.colorKey, c.uiMode);
    const auto it = variants_.find(k);
    if (it != variants_.end()) return it->second;
    if (variants_.size() >= kMostVariants) return nullptr;
    variants_.emplace(k, nullptr);  // asked: once
    if (!variantThread_) {  // the first: its thread
        variantWake_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        variantThread_ = variantWake_ ? CreateThread(nullptr, 0, &Renderer11::variantMain, this, 0, nullptr) : nullptr;
        if (!variantThread_) {
            variantsUsable_ = false;
            LOG_ERROR("Direct3D 11: no thread for the draw shaders of the game's states (error %lu)", GetLastError());
            return nullptr;
        }
    }
    EnterCriticalSection(&variantLock_);
    variantJobs_.push_back(k);
    LeaveCriticalSection(&variantLock_);
    SetEvent(variantWake_);
    return nullptr;
}

DWORD WINAPI Renderer11::variantMain(void* self) {
    static_cast<Renderer11*>(self)->variantLoop();
    return 0;
}

void Renderer11::variantLoop() {
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    if (!compiler) compiler = LoadLibraryA("d3dcompiler_43.dll");
    const auto compile =
        compiler ? reinterpret_cast<pD3DCompile>(reinterpret_cast<void*>(GetProcAddress(compiler, "D3DCompile")))
                 : nullptr;
    const char* source = shaderSource();
    for (;;) {
        WaitForSingleObject(variantWake_, INFINITE);
        for (;;) {
            shader_variants::Key k;
            bool have = false;
            EnterCriticalSection(&variantLock_);
            const bool stop = variantStop_;
            if (!stop && !variantJobs_.empty()) {
                k = variantJobs_.front();
                variantJobs_.erase(variantJobs_.begin());
                have = true;
            }
            LeaveCriticalSection(&variantLock_);
            if (stop) {
                if (compiler) FreeLibrary(compiler);
                return;
            }
            if (!have) break;
            std::vector<uint8_t> code;
            if (compile) {
                const std::vector<shader_variants::Macro> m = shader_variants::macrosOf(k);
                std::vector<D3D_SHADER_MACRO> macros;
                for (const shader_variants::Macro& x : m) macros.push_back({x.name.c_str(), x.value.c_str()});
                macros.push_back({nullptr, nullptr});
                for (const UINT level : {D3DCOMPILE_OPTIMIZATION_LEVEL3, D3DCOMPILE_OPTIMIZATION_LEVEL1}) {
                    ID3DBlob* blob = nullptr;
                    ID3DBlob* errors = nullptr;
                    const HRESULT hr = compile(source, std::strlen(source), "uaro-graphics.hlsl", macros.data(),
                                               nullptr, shader_variants::entryOf(k), variantProfile_, level, 0, &blob,
                                               &errors);
                    if (SUCCEEDED(hr) && blob) {
                        const auto* p = static_cast<const uint8_t*>(blob->GetBufferPointer());
                        code.assign(p, p + blob->GetBufferSize());
                    }
                    drop(blob);
                    drop(errors);
                    if (!code.empty()) break;
                }
            }
            VariantDone d;
            d.key = k;
            if (!code.empty() && FAILED(device_->CreatePixelShader(code.data(), code.size(), nullptr, &d.ps)))
                d.ps = nullptr;  // the device is free-threaded: made here, used by the drawing thread
            std::vector<uint8_t> file;
            EnterCriticalSection(&variantLock_);
            if (d.ps) cacheShaders_.push_back({shader_variants::nameOf(k), variantProfile_, code});
            variantDone_.push_back(d);
            if (d.ps && variantJobs_.empty()) file = shader_cache::pack(variantCacheKey_, cacheShaders_);
            LeaveCriticalSection(&variantLock_);
            if (!file.empty()) win::writeFileAtomic(variantCacheFile_, file.data(), file.size());  // the next start's
        }
    }
}

void Renderer11::collectVariants() {
    if (!variantThread_) return;
    std::vector<VariantDone> done;
    EnterCriticalSection(&variantLock_);
    done.swap(variantDone_);
    LeaveCriticalSection(&variantLock_);
    for (const VariantDone& d : done) {
        const auto it = variants_.find(d.key);
        if (it != variants_.end() && !it->second && d.ps) {
            it->second = d.ps;
            ++variantsMade_;
        } else if (d.ps) {
            d.ps->Release();
        }
    }
    if (variantsMade_ >= variantsLogged_ + 10 || (variantsMade_ && !variantsLogged_)) {
        variantsLogged_ = variantsMade_;
        LOG_INFO("Direct3D 11: %u draw shaders made for the game's own states (kept for the next start)",
                 variantsMade_);
    }
}

void Renderer11::stopVariants() {
    if (variantThread_) {
        EnterCriticalSection(&variantLock_);
        variantStop_ = true;
        LeaveCriticalSection(&variantLock_);
        SetEvent(variantWake_);
        WaitForSingleObject(variantThread_, 30000);  // a shader it is compiling
        CloseHandle(variantThread_);
        variantThread_ = nullptr;
    }
    if (variantWake_) CloseHandle(variantWake_);
    variantWake_ = nullptr;
    for (auto& v : variants_) drop(v.second);
    variants_.clear();
    for (VariantDone& d : variantDone_) drop(d.ps);
    variantDone_.clear();
    if (variantLockMade_) DeleteCriticalSection(&variantLock_);
    variantLockMade_ = variantsUsable_ = false;
}

}  // namespace uagfx::gpu
