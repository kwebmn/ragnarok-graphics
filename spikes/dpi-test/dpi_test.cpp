// THROWAWAY SPIKE for stage 3 of uaRO Graphics: which way of presenting a DXGI flip-model frame is pixel-exact in
// a DPI-unaware process (like uaRO.exe) on a monitor with Windows scaling (e.g. 4K at 175%)?
//   1 = buffer of the window's logical size (what 0.2.0 does): Windows stretches it -> blurry
//   2 = A1: buffer of the window's physical size on the same window
//   3 = A2: a per-monitor-DPI-aware child window (mixed hosting) with its own physical-size swap chain;
//       mouse input must still reach the parent (WM_NCHITTEST -> HTTRANSPARENT)
// Keys 1-3 switch, Esc quits. The title shows the variant, sizes and a click counter; dpi-test.log has details.
// Build: spikes/dpi-test/build.sh
#define _WIN32_WINNT 0x0A00
#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using SetCtxFn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
using SetHostingFn = DPI_HOSTING_BEHAVIOR(WINAPI*)(DPI_HOSTING_BEHAVIOR);
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
using DwmFlushFn = HRESULT(WINAPI*)();

SetCtxFn setThreadCtx = nullptr;
SetHostingFn setHosting = nullptr;
GetDpiForWindowFn getDpiForWindow = nullptr;
GetDpiForMonitorFn getDpiForMonitor = nullptr;
DwmFlushFn dwmFlush = nullptr;

FILE* logFile = nullptr;
void logf(const char* fmt, ...) {
    if (!logFile) return;
    va_list a;
    va_start(a, fmt);
    vfprintf(logFile, fmt, a);
    va_end(a);
    fputc('\n', logFile);
    fflush(logFile);
}

HWND mainWnd = nullptr;
HWND childWnd = nullptr;
int variant = 1;
int parentClicks = 0;
int childClicks = 0;
UINT monitorDpi = 96;

ID3D11Device* device = nullptr;
ID3D11DeviceContext* context = nullptr;
IDXGIFactory2* factory = nullptr;
IDXGISwapChain1* mainChain = nullptr;
IDXGISwapChain1* childChain = nullptr;

// The parent's client size as the calling context sees it: logical (unaware) or physical (per-monitor aware).
SIZE clientSize(HWND hwnd, bool physical) {
    DPI_AWARENESS_CONTEXT old = nullptr;
    if (physical && setThreadCtx) old = setThreadCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (old) setThreadCtx(old);
    return SIZE{rc.right - rc.left, rc.bottom - rc.top};
}

// 5x7 digits for the variant label.
const char* kDigits[3][7] = {
    {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."},
    {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"},
    {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."},
};

void pattern(std::vector<uint32_t>& px, int w, int h) {
    px.assign(size_t(w) * size_t(h), 0xFF000000u);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint32_t c;
            if (y < h / 3)
                c = (y & 1) ? 0xFFFFFFFFu : 0xFF000000u;  // horizontal 1-pixel lines
            else if (x < w / 2)
                c = (x & 1) ? 0xFFFFFFFFu : 0xFF000000u;  // vertical 1-pixel lines
            else
                c = ((x + y) & 1) ? 0xFFFFFFFFu : 0xFF000000u;  // 1-pixel checkerboard
            if (x < 3 || y < 3 || x >= w - 3 || y >= h - 3) c = 0xFFFF0000u;  // red frame: the whole buffer is visible
            px[size_t(y) * size_t(w) + size_t(x)] = c;
        }
    const int block = 14;  // the variant number, big, on a dark box
    for (int y = 0; y < 9 * block && 20 + y < h; ++y)
        for (int x = 0; x < 7 * block && 20 + x < w; ++x) px[size_t(20 + y) * size_t(w) + size_t(20 + x)] = 0xFF303030u;
    for (int r = 0; r < 7; ++r)
        for (int c = 0; c < 5; ++c)
            if (kDigits[variant - 1][r][c] == '#')
                for (int y = 0; y < block; ++y)
                    for (int x = 0; x < block; ++x) {
                        const int px0 = 20 + block + c * block + x, py0 = 20 + block + r * block + y;
                        if (px0 < w && py0 < h) px[size_t(py0) * size_t(w) + size_t(px0)] = 0xFF00FF00u;
                    }
}

void present(IDXGISwapChain1* chain) {
    if (!chain) return;
    ID3D11Texture2D* back = nullptr;
    if (FAILED(chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) return;
    D3D11_TEXTURE2D_DESC d{};
    back->GetDesc(&d);
    std::vector<uint32_t> px;
    pattern(px, int(d.Width), int(d.Height));
    context->UpdateSubresource(back, 0, nullptr, px.data(), d.Width * 4, 0);
    back->Release();
    const HRESULT hr = chain->Present(1, 0);
    logf("present variant %d: buffer %ux%u, hr 0x%08lx", variant, d.Width, d.Height, static_cast<unsigned long>(hr));
}

IDXGISwapChain1* makeChain(HWND hwnd, SIZE s) {
    DXGI_SWAP_CHAIN_DESC1 d{};
    d.Width = UINT(s.cx > 0 ? s.cx : 1);
    d.Height = UINT(s.cy > 0 ? s.cy : 1);
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount = 2;
    d.Scaling = DXGI_SCALING_STRETCH;
    d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1* chain = nullptr;
    const HRESULT hr = factory->CreateSwapChainForHwnd(device, hwnd, &d, nullptr, nullptr, &chain);
    logf("swap chain on %p: %ldx%ld, hr 0x%08lx", static_cast<void*>(hwnd), s.cx, s.cy, static_cast<unsigned long>(hr));
    return SUCCEEDED(hr) ? chain : nullptr;
}

void resizeChain(IDXGISwapChain1* chain, SIZE s) {
    if (!chain) return;
    context->ClearState();
    context->Flush();
    const HRESULT hr = chain->ResizeBuffers(2, UINT(s.cx > 0 ? s.cx : 1), UINT(s.cy > 0 ? s.cy : 1),
                                            DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    logf("resize to %ldx%ld, hr 0x%08lx", s.cx, s.cy, static_cast<unsigned long>(hr));
}

void updateTitle() {
    const SIZE logical = clientSize(mainWnd, false);
    const SIZE physical = clientSize(mainWnd, true);
    const wchar_t* names[3] = {L"как сейчас (кадр размера окна для Windows)", L"A1: кадр в настоящих точках на окне игры",
                               L"A2: своё дочернее окно в настоящих точках"};
    wchar_t t[512];
    swprintf(t, 512, L"Вариант %d — %ls | окно %ldx%ld, на экране %ldx%ld, масштаб %u%% | клики: %d%ls", variant,
             names[variant - 1], logical.cx, logical.cy, physical.cx, physical.cy,
             logical.cx ? unsigned(physical.cx * 100 / logical.cx) : 100u, parentClicks,
             childClicks ? L" (дочернее окно перехватило клики!)" : L"");
    SetWindowTextW(mainWnd, t);
}

void show() {
    const SIZE logical = clientSize(mainWnd, false);
    const SIZE physical = clientSize(mainWnd, true);
    if (variant == 3 && childWnd) {
        // Sized in the child's own (physical) units, from a per-monitor-aware context.
        DPI_AWARENESS_CONTEXT old = setThreadCtx ? setThreadCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) : nullptr;
        SetWindowPos(childWnd, HWND_TOP, 0, 0, physical.cx, physical.cy, SWP_NOACTIVATE);
        RECT rc{};
        GetClientRect(childWnd, &rc);
        if (old) setThreadCtx(old);
        const SIZE childSize{rc.right, rc.bottom};
        RECT seen{};
        GetClientRect(childWnd, &seen);  // the same child as the unaware parent sees it
        logf("child client %ldx%ld (aware view), %ldx%ld (unaware view), parent %ldx%ld / %ldx%ld, dpi %u",
             childSize.cx, childSize.cy, seen.right, seen.bottom, logical.cx, logical.cy, physical.cx, physical.cy,
             getDpiForWindow ? getDpiForWindow(childWnd) : 0);
        ShowWindow(childWnd, SW_SHOWNA);
        if (!childChain) childChain = makeChain(childWnd, childSize);
        else resizeChain(childChain, childSize);
        present(childChain);
    } else {
        if (childWnd) ShowWindow(childWnd, SW_HIDE);
        const SIZE s = variant == 2 ? physical : logical;
        if (!mainChain) mainChain = makeChain(mainWnd, s);
        else resizeChain(mainChain, s);
        present(mainChain);
    }
    updateTitle();
}

// Takes the window's client area from the composed screen in physical pixels and counts, in the line and checker
// areas, the pixels that are neither black nor white: a frame shown pixel for pixel has none, a stretched one many.
void measure() {
    for (int i = 0; i < 3; ++i) {  // let the compositor show the frame just presented
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (dwmFlush) dwmFlush();
        Sleep(120);
    }
    DPI_AWARENESS_CONTEXT oldCtx = setThreadCtx ? setThreadCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) : nullptr;
    RECT rc{};
    GetClientRect(mainWnd, &rc);
    POINT tl{0, 0};
    ClientToScreen(mainWnd, &tl);
    const int w = rc.right, h = rc.bottom;
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(mem, dib);
    const BOOL ok = BitBlt(mem, 0, 0, w, h, screen, tl.x, tl.y, SRCCOPY | CAPTUREBLT);
    GdiFlush();
    if (oldCtx) setThreadCtx(oldCtx);
    const uint32_t* px = static_cast<const uint32_t*>(bits);
    auto luma = [&](int x, int y) {
        const uint32_t c = px[size_t(y) * size_t(w) + size_t(x)];
        return int(((c >> 16) & 255) * 3 + ((c >> 8) & 255) * 6 + (c & 255)) / 10;
    };
    auto region = [&](int x0, int x1, int y0, int y1, double& grey, double& alternating) {
        long n = 0, g = 0, alt = 0, pairs = 0;
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                const int l = luma(x, y);
                ++n;
                if (l > 40 && l < 215) ++g;
                if (x + 1 < x1) {
                    ++pairs;
                    if ((l < 128) != (luma(x + 1, y) < 128)) ++alt;
                }
            }
        grey = n ? 100.0 * g / n : 0;
        alternating = pairs ? 100.0 * alt / pairs : 0;
    };
    double gV = 0, aV = 0, gC = 0, aC = 0, gH = 0, aH = 0;
    if (ok && bits && w > 20 && h > 20) {
        region(w / 10, w * 4 / 10, h * 45 / 100, h * 55 / 100, gV, aV);       // vertical lines
        region(w * 6 / 10, w * 9 / 10, h * 45 / 100, h * 55 / 100, gC, aC);  // checkerboard
        region(w * 3 / 10, w * 7 / 10, h * 10 / 100, h * 25 / 100, gH, aH);  // horizontal lines
    }
    int frame = 0;  // red pixels from the left edge on the middle row: 3 when shown pixel for pixel
    while (ok && bits && frame < w && ((px[size_t(h / 2) * size_t(w) + size_t(frame)] >> 16) & 255) > 128 &&
           ((px[size_t(h / 2) * size_t(w) + size_t(frame)] >> 8) & 255) < 128)
        ++frame;
    const bool exact = ok && gV < 2 && gC < 2 && gH < 2 && frame == 3;
    logf("MEASURE variant %d: capture %s %dx%d at %ld,%ld | grey: lines %.1f%%, checker %.1f%%, rows %.1f%% | "
         "alternating: lines %.1f%%, checker %.1f%% | red frame %d px | %s",
         variant, ok ? "ok" : "FAILED", w, h, tl.x, tl.y, gV, gC, gH, aV, aC, frame,
         exact ? "PIXEL-EXACT" : "RESAMPLED (or capture problem)");
    SelectObject(mem, oldBmp);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

LRESULT CALLBACK childProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_NCHITTEST: return HTTRANSPARENT;  // clicks go to the game window underneath
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: ++childClicks; updateTitle(); return 0;
        case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(h, m, w, l);
}

LRESULT CALLBACK mainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_KEYDOWN:
            if (w >= '1' && w <= '3') {
                variant = int(w - '0');
                show();
            } else if (w == VK_ESCAPE) {
                DestroyWindow(h);
            }
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            ++parentClicks;
            updateTitle();
            return 0;
        case WM_SIZE:
            if (device && w != SIZE_MINIMIZED) show();
            return 0;
        case WM_ERASEBKGND: return 1;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

}  // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string logPath(path);
    logPath = logPath.substr(0, logPath.find_last_of("\\/") + 1) + "dpi-test.log";
    logFile = fopen(logPath.c_str(), "w");

    HMODULE user32 = GetModuleHandleA("user32.dll");
    setThreadCtx = reinterpret_cast<SetCtxFn>(reinterpret_cast<void*>(GetProcAddress(user32, "SetThreadDpiAwarenessContext")));
    setHosting = reinterpret_cast<SetHostingFn>(reinterpret_cast<void*>(GetProcAddress(user32, "SetThreadDpiHostingBehavior")));
    getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForWindow")));
    if (HMODULE dwm = LoadLibraryA("dwmapi.dll"))
        dwmFlush = reinterpret_cast<DwmFlushFn>(reinterpret_cast<void*>(GetProcAddress(dwm, "DwmFlush")));
    if (HMODULE shcore = LoadLibraryA("shcore.dll"))
        getDpiForMonitor = reinterpret_cast<GetDpiForMonitorFn>(reinterpret_cast<void*>(GetProcAddress(shcore, "GetDpiForMonitor")));
    OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof v;
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    if (auto rtl = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion"))))
        rtl(&v);
    logf("uaRO DPI test (stage 3 spike). Windows %lu.%lu build %lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
    logf("APIs: SetThreadDpiAwarenessContext %s, SetThreadDpiHostingBehavior %s, GetDpiForMonitor %s",
         setThreadCtx ? "yes" : "no", setHosting ? "yes" : "no", getDpiForMonitor ? "yes" : "no");

    WNDCLASSW wc{};
    wc.lpfnWndProc = mainProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"uaRODpiTest";
    RegisterClassW(&wc);
    wc.lpfnWndProc = childProc;
    wc.lpszClassName = L"uaRODpiTestChild";
    RegisterClassW(&wc);

    RECT r{0, 0, 960, 540};  // logical, like the game's window: Windows scales it on a high-DPI monitor
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    mainWnd = CreateWindowW(L"uaRODpiTest", L"uaRO DPI test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, nullptr);
    if (getDpiForMonitor) {
        UINT dx = 96, dy = 96;
        if (SUCCEEDED(getDpiForMonitor(MonitorFromWindow(mainWnd, MONITOR_DEFAULTTOPRIMARY), 0, &dx, &dy)))
            monitorDpi = dx;
    }
    {
        const SIZE lg = clientSize(mainWnd, false), ph = clientSize(mainWnd, true);
        logf("scale from the window: %ldx%ld logical = %ldx%ld physical (%ld%%); GetDpiForMonitor says %u (it answers "
             "96 to DPI-unaware programs)", lg.cx, lg.cy, ph.cx, ph.cy, lg.cx ? ph.cx * 100 / lg.cx : 0, monitorDpi);
    }

    // The aware child: per-monitor v2 context and mixed hosting on this thread while it is created.
    if (setThreadCtx && setHosting) {
        const DPI_HOSTING_BEHAVIOR oldHosting = setHosting(DPI_HOSTING_BEHAVIOR_MIXED);
        const DPI_AWARENESS_CONTEXT oldCtx = setThreadCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        childWnd = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"uaRODpiTestChild", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 16,
                                   16, mainWnd, nullptr, inst, nullptr);
        setThreadCtx(oldCtx);
        setHosting(oldHosting);
        logf("child window %p (error %lu), dpi as the child sees it %u", static_cast<void*>(childWnd), GetLastError(),
             childWnd && getDpiForWindow ? getDpiForWindow(childWnd) : 0);
    }

    UINT flags = 0;
    D3D_FEATURE_LEVEL fl{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                   &device, &fl, &context);
    logf("D3D11 device hr 0x%08lx, level 0x%x", static_cast<unsigned long>(hr), unsigned(fl));
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"Не удалось создать устройство Direct3D 11.", L"uaRO DPI test", MB_ICONERROR);
        return 1;
    }
    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* adapter = nullptr;
    device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    dxgiDevice->GetAdapter(&adapter);
    adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&factory));

    MessageBoxW(nullptr,
                L"Сейчас откроется окно с мелким узором. Программа сама покажет варианты 1, 2, 3 и измерит,\n"
                L"какой из них Windows выводит точка в точку (несколько секунд).\n\n"
                L"Пока идёт замер, не двигай окно и ничего им не закрывай. Когда в заголовке появится\n"
                L"«Замер готов», закрой программу (Esc) и пришли файл dpi-test.log, который лежит рядом с ней.",
                L"uaRO DPI test", MB_ICONINFORMATION);

    ShowWindow(mainWnd, SW_SHOW);
    SetForegroundWindow(mainWnd);
    for (int v = 1; v <= 3; ++v) {
        variant = v;
        show();
        measure();
    }
    SetWindowTextW(mainWnd, L"Замер готов — закрой программу (Esc) и пришли dpi-test.log. Клавиши 1-3 — посмотреть варианты.");
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    logf("clicks: parent %d, child %d", parentClicks, childClicks);
    if (childChain) childChain->Release();
    if (mainChain) mainChain->Release();
    if (logFile) fclose(logFile);
    return 0;
}
