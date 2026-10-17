#include "app/system.hpp"

#include <windows.h>

#include <dxgi1_6.h>
#include <tlhelp32.h>

#include <algorithm>

#include "app/text.hpp"
#include "core/settings/compat_flags.hpp"
#include "core/settings/report.hpp"
#include "core/window_plan.hpp"

namespace uagfx::app {
namespace {

using settings::MonitorInfo;
using settings::Size;

struct Found {
    HMONITOR handle;
    MONITORINFOEXW info;
};

BOOL CALLBACK collect(HMONITOR m, HDC, LPRECT, LPARAM out) {
    Found f{m, {}};
    f.info.cbSize = sizeof f.info;
    if (GetMonitorInfoW(m, &f.info)) reinterpret_cast<std::vector<Found>*>(out)->push_back(f);
    return TRUE;
}

// Windows' own copy of a system DLL, from System32: the game folder, searched first, may hold a dxgi.dll of its own
// (ReShade) that the settings app has no business loading (0.6.0 review M8).
HMODULE systemLibrary(const wchar_t* name) {
    wchar_t dir[MAX_PATH];
    const UINT n = GetSystemDirectoryW(dir, MAX_PATH);
    return n && n < MAX_PATH ? LoadLibraryW((std::wstring(dir, n) + L"\\" + name).c_str()) : nullptr;
}

// Windows scaling of a monitor in %: GetDpiForMonitor on Windows 8.1 and newer, else the screen's DPI.
int monitorScale(HMONITOR m) {
    using Fn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static const Fn fn = [] {
        HMODULE shcore = systemLibrary(L"shcore.dll");
        return shcore ? reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(shcore, "GetDpiForMonitor")))
                      : nullptr;
    }();
    UINT x = 96, y = 96;
    if (!fn || FAILED(fn(m, 0 /* MDT_EFFECTIVE_DPI */, &x, &y))) {
        HDC dc = GetDC(nullptr);
        x = UINT(GetDeviceCaps(dc, LOGPIXELSX));
        ReleaseDC(nullptr, dc);
    }
    return MulDiv(int(x), 100, 96);
}


// From DXGI: the monitors whose output shows the HDR10 colour space (HDR on, Windows 10 1703 and newer) and the
// main adapter's driver version.
std::vector<HMONITOR> dxgiFacts(std::string& driver) {
    std::vector<HMONITOR> out;
    HMODULE dxgi = systemLibrary(L"dxgi.dll");
    using CreateFn = HRESULT(WINAPI*)(REFIID, void**);
    const auto create =
        dxgi ? reinterpret_cast<CreateFn>(reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1"))) : nullptr;
    IDXGIFactory1* factory = nullptr;
    if (!create || FAILED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return out;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT a = 0; factory->EnumAdapters1(a, &adapter) == S_OK; ++a) {
        LARGE_INTEGER v{};
        if (a == 0 && SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &v)))
            driver = std::to_string(HIWORD(v.HighPart)) + "." + std::to_string(LOWORD(v.HighPart)) + "." +
                     std::to_string(HIWORD(v.LowPart)) + "." + std::to_string(LOWORD(v.LowPart));
        IDXGIOutput* output = nullptr;
        for (UINT o = 0; adapter->EnumOutputs(o, &output) == S_OK; ++o) {
            IDXGIOutput6* six = nullptr;
            if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void**>(&six)))) {
                DXGI_OUTPUT_DESC1 d{};
                if (SUCCEEDED(six->GetDesc1(&d)) && d.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
                    out.push_back(d.Monitor);
                six->Release();
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return out;
}

// 0.51.1: DXGI tells a 32-bit program no more than 3072 MB of video memory: the display driver's own number from the
// registry (HardwareInformation.qwMemorySize, else .MemorySize) of the card of this name; 0 when not found.
unsigned long long registryVramMb(const std::string& name) {
    const wchar_t* kDisplays = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}";
    HKEY root = nullptr;
    if (name.empty() ||
        RegOpenKeyExW(HKEY_LOCAL_MACHINE, kDisplays, 0, KEY_READ | KEY_WOW64_64KEY, &root) != ERROR_SUCCESS)
        return 0;
    unsigned long long found = 0;
    wchar_t sub[64];
    for (DWORD i = 0; !found; ++i) {
        DWORD n = 64;
        if (RegEnumKeyExW(root, i, sub, &n, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        HKEY key = nullptr;
        if (RegOpenKeyExW(root, sub, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) continue;
        wchar_t desc[256] = {};
        DWORD size = sizeof desc - sizeof(wchar_t), type = 0;
        if (RegQueryValueExW(key, L"DriverDesc", nullptr, &type, reinterpret_cast<BYTE*>(desc), &size) ==
                ERROR_SUCCESS &&
            type == REG_SZ && utf8(desc) == name) {
            unsigned long long bytes = 0;
            DWORD small = 0;
            size = sizeof bytes;
            if (RegQueryValueExW(key, L"HardwareInformation.qwMemorySize", nullptr, &type,
                                 reinterpret_cast<BYTE*>(&bytes), &size) == ERROR_SUCCESS &&
                size == sizeof bytes) {
                found = bytes / (1024 * 1024);
            } else {
                size = sizeof small;
                if (RegQueryValueExW(key, L"HardwareInformation.MemorySize", nullptr, &type,
                                     reinterpret_cast<BYTE*>(&small), &size) == ERROR_SUCCESS && size == sizeof small)
                    found = small / (1024 * 1024);
            }
        }
        RegCloseKey(key);
    }
    RegCloseKey(root);
    return found;
}

// DXGI's capped number made the driver's, where the registry has it.
void realVram(unsigned long long& mb, const std::string& name) {
    if (mb != settings::kVramSeenAtMost) return;
    const unsigned long long r = registryVramMb(name);
    if (r > mb) mb = r;
}

// The frame of a window style at 96 DPI: the game is not DPI-aware, so it measures in those pixels.
settings::Frame frameOf(DWORD style) {
    RECT r{0, 0, 0, 0};
    using ForDpi = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    const auto fn = reinterpret_cast<ForDpi>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi")));
    settings::Frame f;
    if (fn && fn(&r, style, FALSE, 0, 96)) {
        f.width = r.right - r.left;
        f.height = r.bottom - r.top;
    } else if (AdjustWindowRectEx(&r, style, FALSE, 0)) {
        HDC dc = GetDC(nullptr);
        const int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        f.width = MulDiv(r.right - r.left, 96, dpi);
        f.height = MulDiv(r.bottom - r.top, 96, dpi);
    }
    return f;
}

// The frame of the game's window: its own style (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
// registry.md §2) or our driver's normal window (WS_OVERLAPPEDWINDOW, mode = window) - the larger, so that a size
// offered fits either way (0.6.0 review M14).
settings::Frame gameFrame() {
    const settings::Frame own = frameOf(0x02CA0000);
    const settings::Frame ours = frameOf(WS_OVERLAPPEDWINDOW);
    return settings::Frame{std::max(own.width, ours.width), std::max(own.height, ours.height)};
}

bool runsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &e, sizeof e, &size) && e.TokenIsElevated;
    CloseHandle(token);
    return elevated;
}

}  // namespace

SystemInfo probeSystem() {
    SystemInfo s;
    s.windows = win::windowsVersion();
    if (s.windows.major >= 10) {
        s.d3d12 = win::probeD3D12(true);
        s.d3d12Probed = true;
    }
    s.d3d11 = win::probeD3D11(true);
    s.adapters = win::probeAdapters(true);
    realVram(s.d3d12.vramMb, s.d3d12.adapter);  // 0.51.1
    realVram(s.d3d11.vramMb, s.d3d11.adapter);
    for (win::AdapterProbe& a : s.adapters) realVram(a.vramMb, a.name);
    std::vector<Found> found;
    EnumDisplayMonitors(nullptr, nullptr, collect, reinterpret_cast<LPARAM>(&found));
    std::vector<bool> primary;
    for (const Found& f : found) primary.push_back((f.info.dwFlags & MONITORINFOF_PRIMARY) != 0);
    const std::vector<HMONITOR> hdr = dxgiFacts(s.gpuDriver);
    for (size_t i = 0; i < found.size(); ++i) {
        const int at = monitorIndex(primary, int(i));
        if (at < 0) continue;
        const Found& f = found[size_t(at)];
        MonitorInfo m;
        m.primary = primary[size_t(at)];
        DEVMODEW dm{};
        dm.dmSize = sizeof dm;
        if (EnumDisplaySettingsExW(f.info.szDevice, ENUM_CURRENT_SETTINGS, &dm, 0)) {
            m.pixels = {int(dm.dmPelsWidth), int(dm.dmPelsHeight)};
            m.refresh = int(dm.dmDisplayFrequency);
        } else {
            m.pixels = {f.info.rcMonitor.right - f.info.rcMonitor.left, f.info.rcMonitor.bottom - f.info.rcMonitor.top};
        }
        // This app is DPI-aware: monitor rectangles are in real pixels.
        m.work = {f.info.rcWork.right - f.info.rcWork.left, f.info.rcWork.bottom - f.info.rcWork.top};
        m.scale = monitorScale(f.handle);
        m.hdr = std::find(hdr.begin(), hdr.end(), f.handle) != hdr.end();
        s.monitors.push_back(m);
    }
    s.frame = gameFrame();
    s.elevated = runsElevated();
    return s;
}

bool gameRunning(const std::wstring& gameExe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof e;
    bool running = false;
    for (BOOL more = Process32FirstW(snap, &e); more && !running; more = Process32NextW(snap, &e)) {
        if (lstrcmpiW(e.szExeFile, L"uaRO.exe") != 0) continue;
        HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
        if (!p) {
            running = true;  // a game run as administrator: its path cannot be read
            continue;
        }
        wchar_t path[MAX_PATH];
        DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, path, &n))
            running = settings::samePath(utf8(std::wstring(path, n)), utf8(gameExe));
        else
            running = true;
        CloseHandle(p);
    }
    CloseHandle(snap);
    return running;
}

}  // namespace uagfx::app
