#include "win/system_probe.hpp"

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>

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
    IDXGIAdapter1* adapter = nullptr;  // adapter 0 drives the main monitor
    p.hr = factory->EnumAdapters1(0, &adapter);
    if (SUCCEEDED(p.hr)) {
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            p.adapter = utf8(desc.Description);
            p.vendor = desc.VendorId;
            p.device = desc.DeviceId;
            p.vramMb = desc.DedicatedVideoMemory / (1024 * 1024);
        }
        ID3D12Device* device = nullptr;
        p.hr = createDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                            reinterpret_cast<void**>(&device));
        if (SUCCEEDED(p.hr)) {
            p.ok = true;
            device->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return p;
}

}  // namespace uagfx::win
