#include "win/window_mode.hpp"

#include <atomic>
#include <cstdio>
#include <vector>

#include "core/window_plan.hpp"
#include "win/dpi.hpp"
#include "win/iat.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

struct Monitor {
    RECT rect;
    bool primary;
};

BOOL CALLBACK addMonitor(HMONITOR m, HDC, LPRECT, LPARAM data) {
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (GetMonitorInfoA(m, &mi))
        reinterpret_cast<std::vector<Monitor>*>(data)->push_back(Monitor{mi.rcMonitor, (mi.dwFlags & MONITORINFOF_PRIMARY) != 0});
    return TRUE;
}

RECT monitorRect(int index) {
    if (index > 0) {
        std::vector<Monitor> all;
        EnumDisplayMonitors(nullptr, nullptr, addMonitor, reinterpret_cast<LPARAM>(&all));
        std::vector<bool> primary;
        for (const Monitor& m : all) primary.push_back(m.primary);
        const int i = monitorIndex(primary, index);
        if (i >= 0) return all[size_t(i)].rect;
    }
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    GetMonitorInfoA(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    return mi.rcMonitor;
}

// Borderless over the monitor in its real pixels. uaRO.exe is not DPI-aware: in its coordinates a 3840 x 2160 monitor
// at 175 % is 2194 x 1234, and a window of that size came out 3839 x 2159 (0.6.1). False when it did not land on the
// monitor's pixels - Windows before 10 1607, or one that placed it otherwise - and `got` is where it went.
bool placeInPixels(HWND hwnd, int monitor, RECT& got) {
    RealPixels real;
    if (!real.active()) return false;
    const RECT mon = monitorRect(monitor);  // EnumDisplayMonitors and GetMonitorInfo tell real pixels now
    SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    GetWindowRect(hwnd, &got);
    return got.left == mon.left && got.top == mon.top && got.right == mon.right && got.bottom == mon.bottom;
}

const char* actionName(WindowAction a) {
    switch (a) {
        case WindowAction::Window: return "a normal window";
        case WindowAction::Borderless: return "borderless over the monitor";
        case WindowAction::Leave: break;
    }
    return "left as the game made it";
}

}  // namespace

void applyWindowMode(HWND hwnd, const Config& config, bool gameFullscreen, int gameWidth, int gameHeight) {
    if (!hwnd) return;
    const WindowAction action = windowAction(config, gameFullscreen);
    if (action == WindowAction::Leave) {  // ROExt's WindowLock and uaRO Pad keep their window: nothing to fight
        RECT client{};
        GetClientRect(hwnd, &client);
        LOG_INFO("window: %s (mode = %s), client area %ldx%ld, the game draws %dx%d", actionName(action),
                 displayModeToken(config.mode), client.right, client.bottom, gameWidth, gameHeight);
        return;
    }
    const RECT mon = monitorRect(config.monitor);
    const LONG style = GetWindowLongA(hwnd, GWL_STYLE);
    const LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
    const LONG visible = style & WS_VISIBLE;
    if (action == WindowAction::Borderless) {
        SetWindowLongA(hwnd, GWL_STYLE, LONG(WS_POPUP) | visible);
        SetWindowLongA(hwnd, GWL_EXSTYLE,
                       ex & ~LONG(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE | WS_EX_TOPMOST));
        RECT got{};
        if (placeInPixels(hwnd, config.monitor, got)) {
            LOG_INFO("window: %s (mode = %s), %ldx%ld real pixels at %ld,%ld", actionName(action),
                     displayModeToken(config.mode), got.right - got.left, got.bottom - got.top, got.left, got.top);
            return;
        }
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        const int w = config.windowWidth ? config.windowWidth : gameWidth;
        const int h = config.windowHeight ? config.windowHeight : gameHeight;
        const LONG ws = LONG(WS_OVERLAPPEDWINDOW) | visible;
        SetWindowLongA(hwnd, GWL_STYLE, ws);
        RECT r{0, 0, w, h};
        AdjustWindowRectEx(&r, DWORD(ws), FALSE, DWORD(ex));
        const int ow = r.right - r.left;
        const int oh = r.bottom - r.top;
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left + (mon.right - mon.left - ow) / 2,
                     mon.top + (mon.bottom - mon.top - oh) / 2, ow, oh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    RECT now{};
    GetWindowRect(hwnd, &now);
    LOG_INFO("window: %s (mode = %s), %ldx%ld at %ld,%ld", actionName(action), displayModeToken(config.mode),
             now.right - now.left, now.bottom - now.top, now.left, now.top);
}

namespace {

struct Biggest {
    HWND hwnd = nullptr;
    LONG area = 0;
};

BOOL CALLBACK keepBiggest(HWND hwnd, LPARAM p) {
    RECT r;
    if (!IsWindowVisible(hwnd) || !GetWindowRect(hwnd, &r)) return TRUE;
    auto* b = reinterpret_cast<Biggest*>(p);
    const LONG area = (r.right - r.left) * (r.bottom - r.top);
    if (area > b->area) {
        b->area = area;
        b->hwnd = hwnd;
    }
    return TRUE;
}

}  // namespace

namespace {

using ShowFn = BOOL(WINAPI*)(HWND, int);
using PlaceFn = BOOL(WINAPI*)(HWND, HWND, int, int, int, int, UINT);
using MoveFn = BOOL(WINAPI*)(HWND, int, int, int, int, BOOL);
ShowFn g_show = nullptr;
PlaceFn g_place = nullptr;
MoveFn g_move = nullptr;
std::atomic<int> g_placesTold{0};

void tellPlace(const char* call, HWND w, const void* from) {
    if (g_placesTold.fetch_add(1) >= 12) return;
    RECT r{};
    {
        RealPixels real;
        GetWindowRect(w, &r);
    }
    LOG_INFO("window: the game's %s from uaRO.exe 0x%06lx - the window now %ldx%ld real pixels at %ld,%ld", call,
             static_cast<unsigned long>(reinterpret_cast<uintptr_t>(from)), r.right - r.left, r.bottom - r.top, r.left,
             r.top);
}

BOOL WINAPI showWindow(HWND w, int how) {
    const BOOL ok = g_show(w, how);
    char call[32];
    std::snprintf(call, sizeof call, "ShowWindow(%d)", how);
    tellPlace(call, w, __builtin_return_address(0));
    return ok;
}

BOOL WINAPI placeWindow(HWND w, HWND after, int x, int y, int cx, int cy, UINT flags) {
    const BOOL ok = g_place(w, after, x, y, cx, cy, flags);
    char call[96];
    std::snprintf(call, sizeof call, "SetWindowPos(%d, %d, %dx%d, flags 0x%X)", x, y, cx, cy, flags);
    tellPlace(call, w, __builtin_return_address(0));
    return ok;
}

BOOL WINAPI moveWindow(HWND w, int x, int y, int cx, int cy, BOOL repaint) {
    const BOOL ok = g_move(w, x, y, cx, cy, repaint);
    char call[64];
    std::snprintf(call, sizeof call, "MoveWindow(%d, %d, %dx%d)", x, y, cx, cy);
    tellPlace(call, w, __builtin_return_address(0));
    return ok;
}

}  // namespace

void installWindowTrace() {
    hookImport("user32.dll", "ShowWindow", &showWindow, g_show);
    hookImport("user32.dll", "SetWindowPos", &placeWindow, g_place);
    hookImport("user32.dll", "MoveWindow", &moveWindow, g_move);
}

HWND findGameWindow() {
    Biggest found;
    EnumThreadWindows(GetCurrentThreadId(), keepBiggest, reinterpret_cast<LPARAM>(&found));
    return found.hwnd;
}

void blackenGameWindow() {
    const HWND window = findGameWindow();
    if (!window) return;
    // Black for every later erase too, and black now: nothing repaints it until the first present.
    SetClassLongPtrA(window, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(GetStockObject(BLACK_BRUSH)));
    RECT client;
    if (HDC dc = GetClientRect(window, &client) ? GetDC(window) : nullptr) {
        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        ReleaseDC(window, dc);
    }
    ValidateRect(window, nullptr);  // painted: no WM_PAINT left to paint it white again
    LOG_DEBUG("the game's window 0x%p painted black until the first picture", static_cast<void*>(window));
}

}  // namespace uagfx::win
