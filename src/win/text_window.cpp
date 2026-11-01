#include "win/text_window.hpp"

#include <algorithm>
#include <cstdint>
#include <map>

namespace uagfx::win {
namespace {

bool registered(const wchar_t* name) {
    static std::map<std::wstring, ATOM> atoms;
    ATOM& atom = atoms[name];
    if (atom) return true;
    WNDCLASSW c{};
    c.lpfnWndProc = DefWindowProcW;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = name;
    atom = RegisterClassW(&c);
    return atom != 0;
}

}  // namespace

void drawTextWindow(HWND& window, const wchar_t* windowClass, HWND game, const std::wstring& text, TextPlace place) {
    if (!window && registered(windowClass))
        window = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, windowClass, L"",
            WS_POPUP, 0, 0, 1, 1, game, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return;
    // The text white on a dark band, sized by the game's DPI-unaware pixels times the monitor's scale.
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    const int height = std::max(14, GetDeviceCaps(screen, LOGPIXELSY) * 15 / 96);
    HFONT font = CreateFontW(-height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size);
    const int w = size.cx + height, h = size.cy + height / 2;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        auto* px = static_cast<uint32_t*>(bits);
        std::fill(px, px + size_t(w) * size_t(h), 0x00000000u);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, height / 2, height / 4, text.c_str(), int(text.size()));
        // GDI leaves alpha 0: the text's pixels opaque, the band behind it half dark (premultiplied).
        for (size_t i = 0; i < size_t(w) * size_t(h); ++i) {
            const uint32_t c = px[i] & 0xFFFFFF;
            const uint32_t v = std::max({c & 0xFF, (c >> 8) & 0xFF, c >> 16});
            const uint32_t a = std::max<uint32_t>(150, v);
            px[i] = a << 24 | v << 16 | v << 8 | v;
        }
        RECT client{};
        GetClientRect(game, &client);
        POINT at{0, 0};
        ClientToScreen(game, &at);
        POINT pos{at.x + height / 2, at.y + height / 2}, origin{0, 0};
        if (place == TextPlace::TopMiddle) pos.x = at.x + (client.right - client.left - w) / 2;
        SIZE extent{w, h};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(window, screen, &pos, &extent, dc, &origin, 0, &blend, ULW_ALPHA);
        if (!IsWindowVisible(window)) ShowWindow(window, SW_SHOWNOACTIVATE);
        SelectObject(dc, oldBmp);
    }
    if (bmp) DeleteObject(bmp);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

}  // namespace uagfx::win
