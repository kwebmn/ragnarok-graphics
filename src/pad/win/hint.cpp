#include "pad/win/hint.hpp"

namespace uapad::hint {
namespace {

HWND g_window = nullptr;
HFONT g_font = nullptr;
bool g_broken = false;
std::wstring g_text;
RECT g_placed{};

constexpr int kPadX = 14;
constexpr int kHeight = 30;
constexpr int kFromBottom = 70;  // above the client's chat line

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT r;
            GetClientRect(hwnd, &r);
            HBRUSH back = CreateSolidBrush(RGB(24, 28, 40));
            FillRect(dc, &r, back);
            DeleteObject(back);
            SelectObject(dc, g_font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(236, 236, 236));
            DrawTextW(dc, g_text.c_str(), int(g_text.size()), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

HINSTANCE module() {
    HMODULE m = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&proc), &m);
    return m;
}

bool create(HWND game) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = &proc;
    wc.hInstance = module();
    wc.lpszClassName = L"uaRoPadHint";
    if (!RegisterClassExW(&wc)) return false;
    g_font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                               L"uaRoPadHint", L"uaRO Pad", WS_POPUP, 0, 0, 100, kHeight, game, nullptr, module(),
                               nullptr);
    if (!g_window) return false;
    SetLayeredWindowAttributes(g_window, 0, 210, LWA_ALPHA);
    return true;
}

int textWidth(const std::wstring& text) {
    HDC dc = GetDC(g_window);
    HGDIOBJ old = SelectObject(dc, g_font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size);
    SelectObject(dc, old);
    ReleaseDC(g_window, dc);
    return size.cx;
}

}  // namespace

void show(HWND game, const std::wstring& text) {
    if (g_broken || !game) return;
    if (!g_window && !create(game)) {
        g_broken = true;
        return;
    }
    RECT client{};
    GetClientRect(game, &client);
    POINT bottom{(client.right - client.left) / 2, client.bottom - kFromBottom};
    ClientToScreen(game, &bottom);
    const bool changed = text != g_text;
    g_text = text;
    const int w = textWidth(text) + 2 * kPadX;
    const RECT place{bottom.x - w / 2, bottom.y - kHeight, bottom.x - w / 2 + w, bottom.y};
    if (!IsWindowVisible(g_window) || !EqualRect(&place, &g_placed)) {
        g_placed = place;
        SetWindowPos(g_window, HWND_TOP, place.left, place.top, w, kHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    if (changed) InvalidateRect(g_window, nullptr, FALSE);
}

void hide() {
    if (g_window && IsWindowVisible(g_window)) ShowWindow(g_window, SW_HIDE);
    g_text.clear();
}

}  // namespace uapad::hint
