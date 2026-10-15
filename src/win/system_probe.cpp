#include "win/system_probe.hpp"

#include <windows.h>

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "win/gpu/adapters.hpp"

namespace uagfx::win {
namespace {

std::string utf8(const wchar_t* w) {
    char out[256];
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, sizeof out, nullptr, nullptr);
    return n > 0 ? std::string(out) : std::string();
}

// A system DLL: by name, as the game finds it, or Windows' own copy from System32.
HMODULE load(const char* name, bool systemOnly) {
    if (!systemOnly) return LoadLibraryA(name);
    char dir[MAX_PATH];
    const UINT n = GetSystemDirectoryA(dir, MAX_PATH);
    return n && n < MAX_PATH ? LoadLibraryA((std::string(dir, n) + "\\" + name).c_str()) : nullptr;
}

}  // namespace

WindowsVersion windowsVersion() {
    WindowsVersion v;
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    const auto fn = ntdll ? reinterpret_cast<RtlGetVersionFn>(
                                reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")))
                          : nullptr;
    OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof info;
    if (fn && fn(&info) == 0) {
        v.major = info.dwMajorVersion;
        v.minor = info.dwMinorVersion;
        v.build = info.dwBuildNumber;
    }
    return v;
}

D3D12Probe probeD3D12(bool systemOnly) {
    D3D12Probe p;
    HMODULE dxgi = load("dxgi.dll", systemOnly);  // first: d3d12.dll then finds it loaded
    HMODULE d3d12 = load("d3d12.dll", systemOnly);
    if (!d3d12 || !dxgi) {
        p.hr = HRESULT_FROM_WIN32(GetLastError());
        return p;
    }
    const auto createDevice = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
        reinterpret_cast<void*>(GetProcAddress(d3d12, "D3D12CreateDevice")));
    using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    const auto createFactory =
        reinterpret_cast<CreateFactoryFn>(reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1")));
    if (!createDevice || !createFactory) {
        p.hr = E_NOINTERFACE;
        return p;
    }
    IDXGIFactory1* factory = nullptr;
    p.hr = createFactory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
    if (FAILED(p.hr)) return p;
    // 0.50.1: the card the renderer takes - the fast one of several (win/gpu/adapters.hpp), else the first.
    std::vector<IDXGIAdapter1*> adapters = gpu::orderedAdapters(factory);
    if (adapters.empty()) p.hr = DXGI_ERROR_NOT_FOUND;
    for (size_t i = 0; i < adapters.size() && !p.ok; ++i) {
        IDXGIAdapter1* adapter = adapters[i];
        ID3D12Device* device = nullptr;
        const HRESULT hr = createDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                        reinterpret_cast<void**>(&device));
        if (i == 0 || SUCCEEDED(hr)) {  // the card it is on, or the best one when none has Direct3D 12
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                p.adapter = utf8(desc.Description);
                p.vendor = desc.VendorId;
                p.device = desc.DeviceId;
                p.vramMb = desc.DedicatedVideoMemory / (1024 * 1024);
            }
            p.hr = hr;
        }
        if (SUCCEEDED(hr)) {
            p.ok = true;
            device->Release();
        }
    }
    gpu::releaseAdapters(adapters);
    factory->Release();
    return p;
}

D3D11Probe probeD3D11(bool systemOnly) {
    D3D11Probe p;
    HMODULE d3d11 = load("d3d11.dll", systemOnly);
    const auto create = d3d11 ? reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
                                    reinterpret_cast<void*>(GetProcAddress(d3d11, "D3D11CreateDevice")))
                              : nullptr;
    if (!create) {
        p.hr = d3d11 ? E_NOINTERFACE : HRESULT_FROM_WIN32(GetLastError());
        return p;
    }
    // 0.46.1: 11_1 first, as the renderer asks (E_INVALIDARG where the runtime does not know it: Windows 7).
    const D3D_FEATURE_LEVEL levels[4] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                         D3D_FEATURE_LEVEL_10_0};
    ID3D11Device* device = nullptr;
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_10_0;
    const auto make = [&](IDXGIAdapter* a) {
        const D3D_DRIVER_TYPE type = a ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
        HRESULT hr = create(a, type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 4, D3D11_SDK_VERSION, &device,
                            &got, nullptr);
        if (hr == E_INVALIDARG)
            hr = create(a, type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels + 1, 3, D3D11_SDK_VERSION, &device,
                        &got, nullptr);
        return hr;
    };
    // 0.50.1: as the renderer - the fast card of several first.
    p.hr = E_FAIL;
    HMODULE dxgiDll = load("dxgi.dll", systemOnly);
    using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    const auto createFactory = dxgiDll ? reinterpret_cast<CreateFactoryFn>(reinterpret_cast<void*>(
                                             GetProcAddress(dxgiDll, "CreateDXGIFactory1")))
                                       : nullptr;
    IDXGIFactory1* factory = nullptr;
    if (createFactory && SUCCEEDED(createFactory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        std::vector<IDXGIAdapter1*> adapters = gpu::orderedAdapters(factory);
        for (IDXGIAdapter1* a : adapters) {
            p.hr = make(a);
            if (SUCCEEDED(p.hr) && device) break;
            if (device) device->Release();
            device = nullptr;
        }
        gpu::releaseAdapters(adapters);
        factory->Release();
    }
    if (FAILED(p.hr) || !device) p.hr = make(nullptr);
    if (FAILED(p.hr) || !device) return p;
    p.ok = true;
    p.level = unsigned(got);
    IDXGIDevice* dxgi = nullptr;
    IDXGIAdapter* adapter = nullptr;
    DXGI_ADAPTER_DESC desc{};
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi))) &&
        SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&desc))) {
        p.adapter = utf8(desc.Description);
        p.vendor = desc.VendorId;
        p.vramMb = desc.DedicatedVideoMemory / (1024 * 1024);
    }
    if (adapter) adapter->Release();
    if (dxgi) dxgi->Release();
    device->Release();
    return p;
}

std::string featureLevelName(unsigned level) {
    return std::to_string((level >> 12) & 15) + "_" + std::to_string((level >> 8) & 15);
}

}  // namespace uagfx::win
