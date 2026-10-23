#include "app/system.hpp"

#include <windows.h>

#include <dxgi1_6.h>
#include <objbase.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

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

// DXGI's capped number made the driver's, where the registry has it.
void realVram(unsigned long long& mb, const std::string& name) {
    if (mb == settings::kVramSeenAtMost) mb = settings::realVramMb(mb, win::registryVramMb(name));
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
    s.computer = win::probeComputer();  // 2.5.15
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

bool gameRunning(const std::wstring& gameExe) { return running(gameExe); }

bool running(const std::wstring& exe) {
    const std::wstring name = exe.substr(exe.find_last_of(L'\\') + 1);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof e;
    bool found = false;
    for (BOOL more = Process32FirstW(snap, &e); more && !found; more = Process32NextW(snap, &e)) {
        if (lstrcmpiW(e.szExeFile, name.c_str()) != 0) continue;
        HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
        if (!p) {
            found = true;  // a program run as administrator: its path cannot be read
            continue;
        }
        wchar_t path[MAX_PATH];
        DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, path, &n))
            found = settings::samePath(utf8(std::wstring(path, n)), utf8(exe));
        else
            found = true;
        CloseHandle(p);
    }
    CloseHandle(snap);
    return found;
}

// The game's own: a 32-bit process's System32 is SysWOW64 on 64-bit Windows (this app is 32-bit too)
bool hasXAudio2() {
    wchar_t dir[MAX_PATH];
    const UINT n = GetSystemDirectoryW(dir, MAX_PATH);
    return n && n < MAX_PATH &&
           GetFileAttributesW((std::wstring(dir, n) + L"\\xaudio2_9.dll").c_str()) != INVALID_FILE_ATTRIBUTES;
}

void waitMs(unsigned ms) { Sleep(ms); }

bool elevatedNow() { return runsElevated(); }

void markOurStart() { SetEnvironmentVariableW(L"UARO_LAUNCHER", L"1"); }

std::string randomKey() {
    GUID g{};
    if (FAILED(CoCreateGuid(&g))) {  // no RPC: the clock and the process, mixed (a key, not a secret)
        g.Data1 = GetTickCount() ^ (GetCurrentProcessId() << 16);
        g.Data2 = WORD(GetCurrentProcessId());
        const uint64_t mixed = (uint64_t(GetTickCount()) * 0x9e3779b97f4a7c15ull) ^ uint64_t(uintptr_t(&g));
        for (int i = 0; i < 8; ++i) g.Data4[i] = BYTE(mixed >> (8 * i));
    }
    char text[40];
    std::snprintf(text, sizeof text, "%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
                  static_cast<unsigned long>(g.Data1), g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2],
                  g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return text;
}

}  // namespace uagfx::app
