#include "pad/win/game_window.hpp"

#include "pad/core/logic/mouse_gate.hpp"

namespace uapad::gamewindow {
namespace {

WNDPROC g_original = nullptr;
HWND g_subclassed = nullptr;

struct Framed {  // the window as the client made it, while it is borderless
    bool active = false;
    LONG style = 0;
    LONG exStyle = 0;
    RECT rect{};
};
Framed g_framed;
bool g_clipped = false;
// 3.0.3-beta.8: setBlocked; 3.0.9-beta.4: the panel's, the card's; 3.0.11-beta.1: the new minimap's; 3.0.11-beta.7:
// «Довідник»'s (in Blocker's order)
constexpr int kBlockers = 4;
Blocked g_parts[kBlockers];
HWND g_partsWindow = nullptr;  // 3.0.11-beta.24: the window their rectangles are in

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SETCURSOR && LOWORD(lp) != HTCLIENT) return DefWindowProcA(hwnd, msg, wp, lp);
    return CallWindowProcA(g_original, hwnd, msg, wp, lp);
}

constexpr UINT kMoveFlags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE;

}  // namespace

void subclass(HWND game) {
    if (!game || game == g_subclassed) return;
    const LONG_PTR prev = SetWindowLongPtrA(game, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&proc));
    if (prev == 0) return;
    g_original = reinterpret_cast<WNDPROC>(prev);
    g_subclassed = game;
}

bool setBorderless(HWND game, bool on, const WindowPlacement& where) {
    if (!game || IsIconic(game)) return false;
    if (!on) {
        if (!g_framed.active) return true;
        g_framed.active = false;
        SetWindowLongA(game, GWL_STYLE, g_framed.style);
        SetWindowLongA(game, GWL_EXSTYLE, g_framed.exStyle);
        const RECT& r = g_framed.rect;
        return SetWindowPos(game, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, kMoveFlags) != 0;
    }
    if (!g_framed.active) {
        const LONG style = GetWindowLongA(game, GWL_STYLE);
        if ((style & WS_CAPTION) != WS_CAPTION) return false;  // full screen, or no frame to take away
        g_framed.style = style;
        g_framed.exStyle = GetWindowLongA(game, GWL_EXSTYLE);
        if (!GetWindowRect(game, &g_framed.rect)) return false;
    }
    RECT client{};
    if (!GetClientRect(game, &client)) return false;
    const int w = client.right - client.left;
    const int h = client.bottom - client.top;
    POINT origin{0, 0};  // where the picture is now
    ClientToScreen(game, &origin);
    int x = origin.x;
    int y = origin.y;
    if (where.kind == WindowPlacement::Kind::Center) {
        MONITORINFO mi{};
        mi.cbSize = sizeof mi;
        if (GetMonitorInfoA(MonitorFromWindow(game, MONITOR_DEFAULTTONEAREST), &mi)) {
            const RECT& m = mi.rcMonitor;
            x = w >= m.right - m.left ? m.left : m.left + (m.right - m.left - w) / 2;
            y = h >= m.bottom - m.top ? m.top : m.top + (m.bottom - m.top - h) / 2;
        }
    } else if (where.kind == WindowPlacement::Kind::At) {
        x = where.x;
        y = where.y;
    }
    // The minimize box stays: without it the taskbar button cannot minimize the window.
    const LONG frame = WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX;
    SetWindowLongA(game, GWL_STYLE, (g_framed.style & ~frame) | WS_POPUP);
    SetWindowLongA(game, GWL_EXSTYLE,
                   g_framed.exStyle &
                       ~LONG(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    g_framed.active = true;
    return SetWindowPos(game, nullptr, x, y, w, h, kMoveFlags) != 0;
}

void lockCursor(HWND game, bool on) {
    if (!on || !game) {
        releaseCursor();
        return;
    }
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= client.left || client.bottom <= client.top) return;
    POINT tl{client.left, client.top};
    POINT br{client.right, client.bottom};
    ClientToScreen(game, &tl);
    ClientToScreen(game, &br);
    const RECT want{tl.x, tl.y, br.x, br.y};
    RECT now{};
    if (g_clipped && GetClipCursor(&now) && EqualRect(&now, &want)) return;
    g_clipped = ClipCursor(&want) != 0;
}

void releaseCursor() {
    if (!g_clipped) return;
    ClipCursor(nullptr);
    g_clipped = false;
}

void setBlocked(Blocker who, HWND game, const RECT* rect, bool all) {
    Blocked& b = g_parts[static_cast<int>(who)];
    b.on = rect != nullptr;
    b.all = all;
    if (rect) {
        b.left = rect->left;
        b.top = rect->top;
        b.right = rect->right;
        b.bottom = rect->bottom;
        if (game) g_partsWindow = game;
    }
}

int blockedUnder(int x, int y, int width, int height) {
    RECT client{};
    if (!g_partsWindow || !GetClientRect(g_partsWindow, &client)) return -1;
    return blockedAtGame(g_parts, kBlockers, client.right, client.bottom, x, y, width, height);
}

bool cursorOverGame(HWND game, POINT screen) {
    RECT client{};
    POINT p = screen;
    if (!game || !GetClientRect(game, &client) || !ScreenToClient(game, &p) || !PtInRect(&client, p)) return false;
    // 3.0.3-beta.8: the driver's panel of the game windows drawn in the game's picture - a click on it is the
    // panel's, and while it is dragged the held button is not the game's anywhere
    if (blockedAt(g_parts, kBlockers, p.x, p.y) >= 0) return false;
    // Any window of the client's own process counts: a graphics wrapper may present the picture in a window
    // of its own over the game's. A window of another program on top does not.
    const HWND under = WindowFromPoint(screen);
    DWORD pid = 0;
    if (under) GetWindowThreadProcessId(under, &pid);
    return pid == GetCurrentProcessId();
}

void sendFKey(int key, bool down) {
    if (key < 1 || key > 9) return;
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = WORD(VK_F1 + key - 1);
    in.ki.wScan = WORD(MapVirtualKeyA(in.ki.wVk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof in);
}

}  // namespace uapad::gamewindow
