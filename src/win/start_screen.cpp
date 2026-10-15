#include "win/start_screen.hpp"

#include <algorithm>
#include <string>

#include "win/dpi.hpp"

namespace uagfx::win {
namespace {

const wchar_t kClass[] = L"uaRO Graphics start";

struct Panel {
    HWND hwnd = nullptr;
    int width = 0;
    int height = 0;
    int unit = 16;  // the line's height, from the game's window: 36 px on a 4K screen
    start::Progress shown;
    bool painted = false;
    ULONGLONG paintedAt = 0;
};
Panel g_panel;

HFONT font(int height, int weight) {
    return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void fill(HDC dc, const RECT& r, COLORREF colour) {
    HBRUSH brush = CreateSolidBrush(colour);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

void write(HDC dc, HFONT f, COLORREF colour, const std::string& line, RECT r) {
    const std::wstring w(line.begin(), line.end());  // the lines are ASCII
    const HGDIOBJ old = SelectObject(dc, f);
    SetTextColor(dc, colour);
    DrawTextW(dc, w.c_str(), int(w.size()), &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, old);
}

// The panel as it is now: drawn off the screen, then put on it at once.
void paint(HDC dc) {
    const Panel& p = g_panel;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bitmap = mem ? CreateCompatibleBitmap(dc, p.width, p.height) : nullptr;
    if (!bitmap) {
        if (mem) DeleteDC(mem);
        return;
    }
    const HGDIOBJ oldBitmap = SelectObject(mem, bitmap);
    const RECT all{0, 0, p.width, p.height};
    fill(mem, all, RGB(18, 21, 27));
    HBRUSH edge = CreateSolidBrush(RGB(48, 55, 66));
    FrameRect(mem, &all, edge);
    DeleteObject(edge);
    SetBkMode(mem, TRANSPARENT);
    const int u = p.unit, left = u, right = p.width - u;
    HFONT title = font(u * 2 / 3, FW_SEMIBOLD), big = font(u, FW_NORMAL), small = font(u * 2 / 3, FW_NORMAL);
    int y = u * 3 / 4;
    write(mem, title, RGB(130, 142, 158), "uaRO Graphics", RECT{left, y, right, y + u});
    y += u + u / 4;
    write(mem, big, RGB(232, 236, 242), start::line(p.shown), RECT{left, y, right, y + u * 5 / 4});
    y += u * 5 / 4 + u / 2;
    const RECT track{left, y, right, y + std::max(4, u / 4)};
    fill(mem, track, RGB(44, 51, 62));
    RECT done = track;
    done.right = track.left + int(float(track.right - track.left) * start::filled(p.shown) + 0.5f);
    fill(mem, done, RGB(86, 156, 230));
    y = track.bottom + u / 2;
    const std::string hint = start::hint(p.shown);
    if (!hint.empty()) write(mem, small, RGB(130, 142, 158), hint, RECT{left, y, right, y + u});
    BitBlt(dc, 0, 0, p.width, p.height, mem, 0, 0, SRCCOPY);
    DeleteObject(title);
    DeleteObject(big);
    DeleteObject(small);
    SelectObject(mem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);
}

LRESULT CALLBACK panelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {  // once the game's thread handles messages again, before the panel goes
            PAINTSTRUCT ps;
            if (HDC dc = BeginPaint(hwnd, &ps)) {
                if (hwnd == g_panel.hwnd) paint(dc);
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_NCHITTEST: return HTTRANSPARENT;  // the clicks go to the game's window
        default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

}  // namespace

void openStart(HWND game) {
    if (g_panel.hwnd || !game || !IsWindowVisible(game)) return;
    RealPixels real;  // the panel made at the monitor's real pixels, its size and place in them
    RECT client{};
    POINT origin{0, 0};
    if (!GetClientRect(game, &client) || !ClientToScreen(game, &origin)) return;
    const int cw = client.right - client.left, ch = client.bottom - client.top;
    if (cw < 320 || ch < 200) return;
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&panelProc), &self);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = panelProc;
        wc.hInstance = self;
        wc.lpszClassName = kClass;
        registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        if (!registered) return;
    }
    const int u = std::clamp(ch / 60, 12, 40);
    const int w = std::min(cw - 2 * u, u * 22), h = u * 25 / 4;
    HWND hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kClass, L"uaRO Graphics", WS_POPUP,
                                origin.x + (cw - w) / 2, origin.y + (ch - h) / 2, w, h, game, nullptr, self, nullptr);
    if (!hwnd) return;
    g_panel = Panel{};
    g_panel.hwnd = hwnd;
    g_panel.width = w;
    g_panel.height = h;
    g_panel.unit = u;
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
}

void showStart(const start::Progress& p) {
    Panel& g = g_panel;
    if (!g.hwnd) return;
    const ULONGLONG now = GetTickCount64();
    const bool fresh = !g.painted || p.stage != g.shown.stage || (p.total > 0 && p.done >= p.total);
    g.shown = p;
    if (!fresh && now - g.paintedAt < 30) return;
    if (HDC dc = GetDC(g.hwnd)) {  // the game's thread handles no messages meanwhile: painted here and now
        paint(dc);
        ReleaseDC(g.hwnd, dc);
        GdiFlush();
    }
    g.painted = true;
    g.paintedAt = now;
}

void closeStart() {
    if (g_panel.hwnd) DestroyWindow(g_panel.hwnd);
    g_panel = Panel{};
}

}  // namespace uagfx::win
