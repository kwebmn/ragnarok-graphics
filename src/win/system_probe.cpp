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

std::vector<AdapterProbe> probeAdapters(bool systemOnly) {
    std::vector<AdapterProbe> out;
    HMODULE dxgi = load("dxgi.dll", systemOnly);
    HMODULE d3d12 = load("d3d12.dll", systemOnly);  // none before Windows 10
    HMODULE d3d11 = load("d3d11.dll", systemOnly);
    using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    const auto createFactory = dxgi ? reinterpret_cast<CreateFactoryFn>(reinterpret_cast<void*>(
                                          GetProcAddress(dxgi, "CreateDXGIFactory1")))
                                    : nullptr;
    const auto create12 = d3d12 ? reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
                                      reinterpret_cast<void*>(GetProcAddress(d3d12, "D3D12CreateDevice")))
                                : nullptr;
    const auto create11 = d3d11 ? reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
                                      reinterpret_cast<void*>(GetProcAddress(d3d11, "D3D11CreateDevice")))
                                : nullptr;
    IDXGIFactory1* factory = nullptr;
    if (!createFactory || FAILED(createFactory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
        return out;
    std::vector<IDXGIAdapter1*> adapters = gpu::orderedAdapters(factory);
    for (IDXGIAdapter1* a : adapters) {
        AdapterProbe p;
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(a->GetDesc1(&desc))) {
            p.name = utf8(desc.Description);
            p.vendor = desc.VendorId;
            p.vramMb = desc.DedicatedVideoMemory / (1024 * 1024);
        }
        ID3D12Device* device12 = nullptr;
        if (create12 && SUCCEEDED(create12(a, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                           reinterpret_cast<void**>(&device12)))) {
            p.d3d12 = true;
            device12->Release();
        }
        const D3D_FEATURE_LEVEL levels[3] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        ID3D11Device* device11 = nullptr;
        D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_10_0;
        if (create11 && SUCCEEDED(create11(a, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, levels, 3, D3D11_SDK_VERSION,
                                           &device11, &got, nullptr))) {
            p.d3d11Level = unsigned(got);
            device11->Release();
        }
        out.push_back(p);
    }
    gpu::releaseAdapters(adapters);
    factory->Release();
    return out;
}

unsigned long long registryVramMb(const std::string& name) {
    using Open = LONG(WINAPI*)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
    using Enum = LONG(WINAPI*)(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME);
    using Query = LONG(WINAPI*)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
    using Close = LONG(WINAPI*)(HKEY);
    HMODULE advapi = LoadLibraryA("advapi32.dll");
    if (!advapi) return 0;
    const auto open = reinterpret_cast<Open>(reinterpret_cast<void*>(GetProcAddress(advapi, "RegOpenKeyExW")));
    const auto list = reinterpret_cast<Enum>(reinterpret_cast<void*>(GetProcAddress(advapi, "RegEnumKeyExW")));
    const auto query = reinterpret_cast<Query>(reinterpret_cast<void*>(GetProcAddress(advapi, "RegQueryValueExW")));
    const auto close = reinterpret_cast<Close>(reinterpret_cast<void*>(GetProcAddress(advapi, "RegCloseKey")));
    const wchar_t* kDisplays = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}";
    HKEY root = nullptr;
    unsigned long long found = 0;
    if (open && list && query && close && !name.empty() &&
        open(HKEY_LOCAL_MACHINE, kDisplays, 0, KEY_READ | KEY_WOW64_64KEY, &root) == ERROR_SUCCESS) {
        wchar_t sub[64];
        for (DWORD i = 0; !found; ++i) {
            DWORD n = 64;
            if (list(root, i, sub, &n, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            HKEY key = nullptr;
            if (open(root, sub, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) continue;
            wchar_t desc[256] = {};
            DWORD size = sizeof desc - sizeof(wchar_t), type = 0;
            if (query(key, L"DriverDesc", nullptr, &type, reinterpret_cast<BYTE*>(desc), &size) == ERROR_SUCCESS &&
                type == REG_SZ && utf8(desc) == name) {
                unsigned long long bytes = 0;
                DWORD small = 0;
                size = sizeof bytes;
                if (query(key, L"HardwareInformation.qwMemorySize", nullptr, &type, reinterpret_cast<BYTE*>(&bytes),
                          &size) == ERROR_SUCCESS &&
                    size == sizeof bytes) {
                    found = bytes / (1024 * 1024);
                } else {
                    size = sizeof small;
                    if (query(key, L"HardwareInformation.MemorySize", nullptr, &type, reinterpret_cast<BYTE*>(&small),
                              &size) == ERROR_SUCCESS &&
                        size == sizeof small)
                        found = small / (1024 * 1024);
                }
            }
            close(key);
        }
        close(root);
    }
    FreeLibrary(advapi);
    return found;
}

std::string featureLevelName(unsigned level) {
    return std::to_string((level >> 12) & 15) + "_" + std::to_string((level >> 8) & 15);
}

}  // namespace uagfx::win
