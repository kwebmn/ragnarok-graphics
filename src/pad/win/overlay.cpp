#include "pad/win/overlay.hpp"

#include <algorithm>

namespace uapad::overlay {
namespace {

constexpr int kWidth = 600;
constexpr int kHeader = 54;
constexpr int kTabs = 42;  // the tab strip under the title
constexpr int kRow = 32;
constexpr int kFooter = 104;
constexpr int kLeft = 24;   // where the labels start
constexpr int kArrow = 28;  // the clickable ◄ / ► boxes at both ends of the value column
constexpr COLORREF kBack = RGB(28, 32, 40);
constexpr COLORREF kLine = RGB(70, 80, 96);
constexpr COLORREF kText = RGB(222, 226, 232);
constexpr COLORREF kDim = RGB(150, 158, 170);
constexpr COLORREF kPick = RGB(52, 74, 112);
constexpr COLORREF kAccent = RGB(255, 208, 96);

HWND g_window = nullptr;
View g_view;
std::vector<MouseEvent> g_clicks;
HFONT g_font = nullptr;
HFONT g_bold = nullptr;
RECT g_placed{};       // where the window was last put
int g_cursorShows = 0;  // ShowCursor(TRUE) calls to undo when the menu goes away
bool g_shown = false;   // the menu wants the window on screen (the system may hide it with the game)
bool g_broken = false;  // creating the window failed once: do not retry every frame

size_t slots(const View& v) { return v.rowSlots > v.rows.size() ? v.rowSlots : v.rows.size(); }
int rowsTop() { return kHeader + kTabs; }
int footerTop(const View& v) { return rowsTop() + int(slots(v)) * kRow; }
int heightFor(const View& v) { return footerTop(v) + kFooter; }
int valueColumn(const View& v) { return kLeft + v.labelWidth; }

RECT tabBox(const View& v, size_t i) {
    const int n = v.tabs.empty() ? 1 : int(v.tabs.size());
    const int w = (kWidth - 32) / n;
    return RECT{16 + int(i) * w, kHeader, 16 + int(i + 1) * w - 6, kHeader + kTabs - 8};
}

RECT saveBox(const View& v) {
    const int y = footerTop(v) + 60;
    return RECT{kWidth - 300, y, kWidth - 160, y + 34};
}

RECT cancelBox(const View& v) {
    const RECT s = saveBox(v);
    return RECT{s.right + 12, s.top, s.right + 152, s.bottom};
}

void text(HDC dc, const std::wstring& s, RECT r, COLORREF color, UINT flags) {
    SetTextColor(dc, color);
    DrawTextW(dc, s.c_str(), int(s.size()), &r, flags | DT_SINGLELINE | DT_NOPREFIX);
}

void fill(HDC dc, RECT r, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void frame(HDC dc, RECT r, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    FrameRect(dc, &r, b);
    DeleteObject(b);
}

void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC screen = BeginPaint(hwnd, &ps);
    RECT all;
    GetClientRect(hwnd, &all);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, all.right, all.bottom);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    SetBkMode(dc, TRANSPARENT);
    fill(dc, all, kBack);
    frame(dc, all, kLine);

    HGDIOBJ oldFont = SelectObject(dc, g_bold);
    text(dc, g_view.title, RECT{kLeft, 0, kWidth - 16, kHeader}, kText, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    SelectObject(dc, g_font);
    for (size_t i = 0; i < g_view.tabs.size(); ++i) {
        const RECT box = tabBox(g_view, i);
        const bool shown = i == g_view.tab;
        if (shown) fill(dc, box, kPick);
        frame(dc, box, kLine);
        text(dc, g_view.tabs[i], box, shown ? kAccent : kDim, DT_CENTER | DT_VCENTER);
    }
    const int values = valueColumn(g_view);
    for (size_t i = 0; i < g_view.rows.size(); ++i) {
        const View::Row& row = g_view.rows[i];
        const int top = rowsTop() + int(i) * kRow;
        const bool picked = i == g_view.selected;
        if (picked) fill(dc, RECT{1, top, kWidth - 1, top + kRow}, kPick);
        if (row.command) {  // "reset all"
            text(dc, row.label, RECT{kLeft, top, kWidth - 16, top + kRow}, picked ? kAccent : kDim,
                 DT_LEFT | DT_VCENTER);
            continue;
        }
        text(dc, row.label, RECT{kLeft, top, values - 8, top + kRow}, picked ? kAccent : kText,
             DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        text(dc, L"◄", RECT{values, top, values + kArrow, top + kRow}, picked ? kAccent : kDim, DT_CENTER | DT_VCENTER);
        text(dc, row.value, RECT{values + kArrow + 6, top, kWidth - kArrow - 22, top + kRow}, kText,
             DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        text(dc, L"►", RECT{kWidth - kArrow - 16, top, kWidth - 16, top + kRow}, picked ? kAccent : kDim,
             DT_CENTER | DT_VCENTER);
    }
    const int footer = footerTop(g_view);
    RECT help{kLeft, footer + 6, kWidth - 16, footer + 54};
    SetTextColor(dc, kDim);
    DrawTextW(dc, g_view.help.c_str(), int(g_view.help.size()), &help, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
    const RECT save = saveBox(g_view);
    const RECT cancel = cancelBox(g_view);
    fill(dc, save, kPick);
    frame(dc, save, kLine);
    text(dc, g_view.save, save, kText, DT_CENTER | DT_VCENTER);
    frame(dc, cancel, kLine);
    text(dc, g_view.cancel, cancel, kText, DT_CENTER | DT_VCENTER);

    BitBlt(screen, 0, 0, all.right, all.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(hwnd, &ps);
}

void click(int x, int y) {
    const POINT p{x, y};
    const RECT save = saveBox(g_view);
    const RECT cancel = cancelBox(g_view);
    if (PtInRect(&save, p)) {
        g_clicks.push_back(MouseEvent{Click::Save, 0});
        return;
    }
    if (PtInRect(&cancel, p)) {
        g_clicks.push_back(MouseEvent{Click::Cancel, 0});
        return;
    }
    for (size_t i = 0; i < g_view.tabs.size(); ++i) {
        const RECT box = tabBox(g_view, i);
        if (PtInRect(&box, p)) {
            g_clicks.push_back(MouseEvent{Click::Tab, i});
            return;
        }
    }
    if (y < rowsTop()) return;
    const size_t row = size_t((y - rowsTop()) / kRow);
    if (row >= g_view.rows.size()) return;
    const bool valueRow = !g_view.rows[row].command;
    const int values = valueColumn(g_view);
    if (valueRow && x >= values && x < values + kArrow)
        g_clicks.push_back(MouseEvent{Click::Previous, row});
    else if (valueRow && x >= kWidth - kArrow - 16)
        g_clicks.push_back(MouseEvent{Click::Next, row});
    else
        g_clicks.push_back(MouseEvent{Click::Select, row});
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;  // the game keeps the focus
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint(hwnd);
            return 0;
        case WM_SETCURSOR:
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        case WM_SHOWWINDOW:
            // the game was restored: a closed menu stays closed
            if (wp && lp == SW_PARENTOPENING && !g_shown) return 0;
            return DefWindowProcW(hwnd, msg, wp, lp);
        case WM_LBUTTONDOWN:
            click(short(LOWORD(lp)), short(HIWORD(lp)));
            return 0;
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
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = &proc;
        wc.hInstance = module();
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"uaRoPadMenu";
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }
    if (!g_font)
        g_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!g_bold)
        g_bold = CreateFontW(-21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED, L"uaRoPadMenu", L"uaRO Pad",
                               WS_POPUP, 0, 0, kWidth, 200, game, nullptr, module(), nullptr);
    if (!g_window) return false;
    SetLayeredWindowAttributes(g_window, 0, 242, LWA_ALPHA);
    return true;
}

}  // namespace

bool show(HWND game, const View& view) {
    if (g_broken) return false;
    if (!g_window && !create(game)) {
        g_broken = true;
        return false;
    }
    RECT client{};
    GetClientRect(game, &client);
    POINT centre{(client.right - client.left) / 2, (client.bottom - client.top) / 2};
    ClientToScreen(game, &centre);
    const int h = heightFor(view);
    const RECT place{centre.x - kWidth / 2, centre.y - h / 2, centre.x - kWidth / 2 + kWidth, centre.y - h / 2 + h};
    const bool opening = !g_shown;
    const bool hidden = opening || !IsWindowVisible(g_window);
    const bool changed = hidden || !(view == g_view);
    g_view = view;
    if (opening) {
        g_shown = true;
        g_clicks.clear();  // nothing from an earlier time the menu was open
        // The game may hide the Windows cursor with ShowCursor(FALSE): the menu needs one.
        g_cursorShows = 0;
        while (g_cursorShows < 50 && ShowCursor(TRUE) < 0) ++g_cursorShows;
        if (g_cursorShows < 50) ++g_cursorShows;  // the call that reached zero counts too
    }
    if (hidden || !EqualRect(&place, &g_placed)) {  // opened, hidden by the system, or the game window moved
        g_placed = place;
        SetWindowPos(g_window, HWND_TOP, place.left, place.top, kWidth, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    if (changed) InvalidateRect(g_window, nullptr, FALSE);
    return true;
}

void hide() {
    if (g_window) ShowWindow(g_window, SW_HIDE);
    if (g_shown) {
        while (g_cursorShows > 0) {  // back to the game's own cursor count
            ShowCursor(FALSE);
            --g_cursorShows;
        }
    }
    g_shown = false;
    g_clicks.clear();
}

std::vector<MouseEvent> takeClicks() {
    std::vector<MouseEvent> out;
    out.swap(g_clicks);
    return out;
}

}  // namespace uapad::overlay
