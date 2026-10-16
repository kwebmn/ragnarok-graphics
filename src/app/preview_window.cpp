#include "app/preview_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>

namespace uagfx::app {
namespace {

struct Picture {
    HBITMAP bitmap = nullptr;
    int width = 0, height = 0;
    std::wstring caption;
};

struct Viewer {
    HWND hwnd = nullptr;
    std::vector<Picture> pictures;
    std::wstring title;
    double zoom = 1.0;       // over the scale that fits a picture into its panel
    double cx = 0, cy = 0;   // the picture's point in the panels' middle
    bool dragging = false;
    POINT last{};
    HFONT font = nullptr;
};
Viewer g_viewer;

constexpr int kCaption = 24;
constexpr int kGap = 8;

void freePictures() {
    for (Picture& p : g_viewer.pictures)
        if (p.bitmap) DeleteObject(p.bitmap);
    g_viewer.pictures.clear();
}

// The grid that shows the pictures largest: 4 to a row, 2 by 2 or one under another.
void layout(const RECT& client, int& cols, int& rows, int& pw, int& ph) {
    const int n = std::max<int>(1, int(g_viewer.pictures.size()));
    const int iw = g_viewer.pictures.empty() ? 1 : std::max(1, g_viewer.pictures[0].width);
    const int ih = g_viewer.pictures.empty() ? 1 : std::max(1, g_viewer.pictures[0].height);
    double best = -1;
    for (int c = 1; c <= n; ++c) {
        const int r = (n + c - 1) / c;
        const int w = (int(client.right) - kGap * (c + 1)) / c;
        const int h = (int(client.bottom) - kGap * (r + 1)) / r - kCaption;
        if (w <= 0 || h <= 0) continue;
        const double s = std::min(double(w) / iw, double(h) / ih);
        if (s > best) {
            best = s;
            cols = c;
            rows = r;
            pw = w;
            ph = h;
        }
    }
    if (best < 0) cols = rows = 1, pw = ph = 1;
}

double fitScale(int pw, int ph) {
    if (g_viewer.pictures.empty()) return 1.0;
    const Picture& p = g_viewer.pictures[0];
    return std::min(double(pw) / std::max(1, p.width), double(ph) / std::max(1, p.height));
}

void clampView(int pw, int ph) {
    if (g_viewer.pictures.empty()) return;
    const Picture& p = g_viewer.pictures[0];
    const double s = fitScale(pw, ph) * g_viewer.zoom;
    const double hw = pw / (2 * s), hh = ph / (2 * s);  // half the view, in the picture's pixels
    g_viewer.cx = hw * 2 >= p.width ? p.width / 2.0 : std::clamp(g_viewer.cx, hw, p.width - hw);
    g_viewer.cy = hh * 2 >= p.height ? p.height / 2.0 : std::clamp(g_viewer.cy, hh, p.height - hh);
}

void retitle() {
    wchar_t zoom[32];
    swprintf(zoom, 32, L" - %d%%", int(std::lround(g_viewer.zoom * 100)));
    SetWindowTextW(g_viewer.hwnd, (g_viewer.title + zoom).c_str());
}

void paint(HDC dc, const RECT& client) {
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP back = CreateCompatibleBitmap(dc, client.right, client.bottom);
    HGDIOBJ oldBack = SelectObject(mem, back);
    HBRUSH ground = CreateSolidBrush(RGB(30, 32, 38));
    FillRect(mem, &client, ground);
    int cols = 1, rows = 1, pw = 1, ph = 1;
    layout(client, cols, rows, pw, ph);
    clampView(pw, ph);
    const double s = fitScale(pw, ph) * g_viewer.zoom;
    HDC src = CreateCompatibleDC(dc);
    SelectObject(mem, g_viewer.font);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(220, 222, 228));
    for (size_t i = 0; i < g_viewer.pictures.size(); ++i) {
        const Picture& p = g_viewer.pictures[i];
        const int x0 = kGap + int(i % size_t(cols)) * (pw + kGap);
        const int y0 = kGap + int(i / size_t(cols)) * (ph + kCaption + kGap);
        RECT cap{x0, y0, x0 + pw, y0 + kCaption};
        DrawTextW(mem, p.caption.c_str(), int(p.caption.size()), &cap, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (!p.bitmap) continue;
        // What of the picture the panel shows, and where on the panel it lands (the whole one is centred).
        const double vw = pw / s, vh = ph / s;
        double sx = g_viewer.cx - vw / 2, sy = g_viewer.cy - vh / 2;
        double sw = vw, sh = vh;
        int dx = x0, dy = y0 + kCaption, dw = pw, dh = ph;
        if (vw >= p.width) {
            sx = 0;
            sw = p.width;
            dw = int(std::lround(p.width * s));
            dx = x0 + (pw - dw) / 2;
        }
        if (vh >= p.height) {
            sy = 0;
            sh = p.height;
            dh = int(std::lround(p.height * s));
            dy = y0 + kCaption + (ph - dh) / 2;
        }
        HGDIOBJ old = SelectObject(src, p.bitmap);
        SetStretchBltMode(mem, s >= 1.0 ? COLORONCOLOR : HALFTONE);  // zoomed in: pixel by pixel
        SetBrushOrgEx(mem, 0, 0, nullptr);
        StretchBlt(mem, dx, dy, dw, dh, src, int(sx), int(sy), std::max(1, int(std::lround(sw))),
                   std::max(1, int(std::lround(sh))), SRCCOPY);
        SelectObject(src, old);
    }
    DeleteDC(src);
    BitBlt(dc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBack);
    DeleteObject(back);
    DeleteObject(ground);
    DeleteDC(mem);
}

// The picture's point under the mouse (client coordinates), and the panel's size.
bool pictureAt(POINT m, double& px, double& py, int& pw, int& ph) {
    RECT client;
    GetClientRect(g_viewer.hwnd, &client);
    int cols = 1, rows = 1;
    layout(client, cols, rows, pw, ph);
    const int col = std::clamp((int(m.x) - kGap) / (pw + kGap), 0, cols - 1);
    const int row = std::clamp((int(m.y) - kGap) / (ph + kCaption + kGap), 0, rows - 1);
    const int x0 = kGap + col * (pw + kGap), y0 = kGap + row * (ph + kCaption + kGap) + kCaption;
    const double s = fitScale(pw, ph) * g_viewer.zoom;
    px = g_viewer.cx + (m.x - (x0 + pw / 2.0)) / s;
    py = g_viewer.cy + (m.y - (y0 + ph / 2.0)) / s;
    return true;
}

void zoomAt(POINT m, double factor) {
    double px = 0, py = 0;
    int pw = 1, ph = 1;
    pictureAt(m, px, py, pw, ph);
    const double before = g_viewer.zoom;
    g_viewer.zoom = std::clamp(g_viewer.zoom * factor, 1.0, 64.0);
    const double k = before / g_viewer.zoom;  // the point under the mouse stays under it
    g_viewer.cx = px + (g_viewer.cx - px) * k;
    g_viewer.cy = py + (g_viewer.cy - py) * k;
    clampView(pw, ph);
    retitle();
    InvalidateRect(g_viewer.hwnd, nullptr, FALSE);
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT client;
            GetClientRect(hwnd, &client);
            if (client.right > 0 && client.bottom > 0) paint(dc, client);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_SIZE:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_MOUSEWHEEL: {
            POINT m{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &m);
            zoomAt(m, std::pow(1.25, GET_WHEEL_DELTA_WPARAM(wp) / double(WHEEL_DELTA)));
            return 0;
        }
        case WM_LBUTTONDOWN:
            g_viewer.dragging = true;
            g_viewer.last = POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            SetCapture(hwnd);
            return 0;
        case WM_MOUSEMOVE:
            if (g_viewer.dragging) {
                const POINT m{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                RECT client;
                GetClientRect(hwnd, &client);
                int cols = 1, rows = 1, pw = 1, ph = 1;
                layout(client, cols, rows, pw, ph);
                const double s = fitScale(pw, ph) * g_viewer.zoom;
                g_viewer.cx -= (m.x - g_viewer.last.x) / s;
                g_viewer.cy -= (m.y - g_viewer.last.y) / s;
                g_viewer.last = m;
                clampView(pw, ph);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            g_viewer.dragging = false;
            ReleaseCapture();
            return 0;
        case WM_LBUTTONDBLCLK:
            g_viewer.zoom = 1.0;
            retitle();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN: {
            RECT client;
            GetClientRect(hwnd, &client);
            const POINT middle{client.right / 2, client.bottom / 2};
            if (wp == VK_ADD || wp == VK_OEM_PLUS) zoomAt(middle, 2.0);
            if (wp == VK_SUBTRACT || wp == VK_OEM_MINUS) zoomAt(middle, 0.5);
            if (wp == '0' || wp == VK_NUMPAD0) zoomAt(middle, 1.0 / g_viewer.zoom);
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY:
            freePictures();
            if (g_viewer.font) DeleteObject(g_viewer.font);
            g_viewer = Viewer{};
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

}  // namespace

void showPreviewWindow(HWND owner, const std::vector<std::pair<std::wstring, std::wstring>>& files,
                       const std::wstring& title) {
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        wc.lpszClassName = L"uaRoGraphicsPreview";
        if (!RegisterClassExW(&wc)) return;
        registered = true;
    }
    freePictures();
    for (const auto& f : files) {
        Picture p;
        p.caption = f.second;
        p.bitmap = static_cast<HBITMAP>(
            LoadImageW(nullptr, f.first.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION));
        if (p.bitmap) {
            BITMAP info{};
            GetObjectW(p.bitmap, sizeof info, &info);
            p.width = int(info.bmWidth);
            p.height = int(info.bmHeight);
        }
        g_viewer.pictures.push_back(std::move(p));
    }
    g_viewer.title = title;
    g_viewer.zoom = 1.0;
    if (!g_viewer.pictures.empty()) {
        g_viewer.cx = g_viewer.pictures[0].width / 2.0;
        g_viewer.cy = g_viewer.pictures[0].height / 2.0;
    }
    if (!g_viewer.hwnd) {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const int w = (work.right - work.left) * 4 / 5, h = (work.bottom - work.top) * 4 / 5;
        g_viewer.hwnd = CreateWindowExW(0, L"uaRoGraphicsPreview", title.c_str(), WS_OVERLAPPEDWINDOW,
                                        work.left + (work.right - work.left - w) / 2,
                                        work.top + (work.bottom - work.top - h) / 2, w, h, owner, nullptr, inst,
                                        nullptr);
        if (!g_viewer.hwnd) return;
        NONCLIENTMETRICSW m{};
        m.cbSize = sizeof m;
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0);
        g_viewer.font = CreateFontIndirectW(&m.lfMessageFont);
    }
    retitle();
    ShowWindow(g_viewer.hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(g_viewer.hwnd);
    InvalidateRect(g_viewer.hwnd, nullptr, FALSE);
}

}  // namespace uagfx::app
