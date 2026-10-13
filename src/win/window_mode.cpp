#include "win/window_mode.hpp"

#include <vector>

#include "win/log.hpp"

namespace uagfx::win {
namespace {

BOOL CALLBACK addMonitor(HMONITOR m, HDC, LPRECT, LPARAM data) {
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (GetMonitorInfoA(m, &mi)) reinterpret_cast<std::vector<RECT>*>(data)->push_back(mi.rcMonitor);
    return TRUE;
}

RECT monitorRect(int index) {
    if (index > 0) {
        std::vector<RECT> all;
        EnumDisplayMonitors(nullptr, nullptr, addMonitor, reinterpret_cast<LPARAM>(&all));
        if (size_t(index) < all.size()) return all[size_t(index)];
    }
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    GetMonitorInfoA(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    return mi.rcMonitor;
}

}  // namespace

void applyWindowMode(HWND hwnd, const Config& config, int gameWidth, int gameHeight) {
    if (!hwnd) return;
    const RECT mon = monitorRect(config.monitor);
    const LONG style = GetWindowLongA(hwnd, GWL_STYLE);
    const LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
    const LONG visible = style & WS_VISIBLE;
    if (config.mode == DisplayMode::Borderless) {
        SetWindowLongA(hwnd, GWL_STYLE, LONG(WS_POPUP) | visible);
        SetWindowLongA(hwnd, GWL_EXSTYLE,
                       ex & ~LONG(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE | WS_EX_TOPMOST));
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
    LOG_INFO("window: %s, %ldx%ld at %ld,%ld", displayModeToken(config.mode), now.right - now.left,
             now.bottom - now.top, now.left, now.top);
}

}  // namespace uagfx::win
