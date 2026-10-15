#include "win/gpu/adapters.hpp"

#include <dxgi1_6.h>

namespace uagfx::gpu {
namespace {

std::string g_chosen;  // set once before the renderer is made

AdapterFacts factsOf(IDXGIAdapter1* a) {
    AdapterFacts f;
    DXGI_ADAPTER_DESC1 d{};
    if (FAILED(a->GetDesc1(&d))) return f;
    char name[256];
    const int n = WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, sizeof name, nullptr, nullptr);
    f.name = n > 0 ? std::string(name) : std::string();
    f.vendor = d.VendorId;
    f.vramMb = d.DedicatedVideoMemory / (1024 * 1024);
    f.software = (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 || d.VendorId == 0x1414;  // the Basic Render Driver
    return f;
}

}  // namespace

std::vector<IDXGIAdapter1*> orderedAdapters(IDXGIFactory1* factory, std::vector<AdapterFacts>* all) {
    std::vector<IDXGIAdapter1*> found;
    std::vector<AdapterFacts> facts;
    bool byPreference = false;
    IDXGIFactory6* f6 = nullptr;  // Windows 10 1803 and newer
    if (factory && SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory6), reinterpret_cast<void**>(&f6)))) {
        IDXGIAdapter1* a = nullptr;
        const auto next = [&](UINT i) {
            return f6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter1),
                                                  reinterpret_cast<void**>(&a)) == S_OK;
        };
        for (UINT i = 0; next(i); ++i) {
            found.push_back(a);
            facts.push_back(factsOf(a));
        }
        f6->Release();
        byPreference = !found.empty();
    }
    if (found.empty() && factory) {
        IDXGIAdapter1* a = nullptr;
        for (UINT i = 0; factory->EnumAdapters1(i, &a) == S_OK; ++i) {
            found.push_back(a);
            facts.push_back(factsOf(a));
        }
    }
    std::vector<IDXGIAdapter1*> out;
    for (size_t i : adapterOrder(facts, byPreference, g_chosen)) {
        found[i]->AddRef();
        out.push_back(found[i]);
    }
    releaseAdapters(found);
    if (all) *all = std::move(facts);
    return out;
}

void setChosenAdapter(const std::string& name) { g_chosen = name; }

void releaseAdapters(std::vector<IDXGIAdapter1*>& adapters) {
    for (IDXGIAdapter1* a : adapters)
        if (a) a->Release();
    adapters.clear();
}

}  // namespace uagfx::gpu
