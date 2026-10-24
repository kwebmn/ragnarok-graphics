#include "win/fps_overlay.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace uagfx::win {
namespace {

const wchar_t kClass[] = L"uaroGraphicsFps";

ATOM windowClass() {
    static ATOM atom = 0;
    if (atom) return atom;
    WNDCLASSW c{};
    c.lpfnWndProc = DefWindowProcW;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = kClass;
    atom = RegisterClassW(&c);
    return atom;
}

}  // namespace

FpsOverlay::~FpsOverlay() {
    if (window_) DestroyWindow(window_);
}

void FpsOverlay::setGpu(const GpuTiming* t) {
    gpu_.clear();
    shown_ = t ? t->shown : 0.0;
    if (!t || t->gpuMs < 0.0) return;
    wchar_t text[96];
    if (t->upscaleMs > 0.05)
        std::swprintf(text, 96, L"  ·  GPU %.1f ms (%hs %.1f)", t->gpuMs, t->upscaler.c_str(), t->upscaleMs);
    else
        std::swprintf(text, 96, L"  ·  GPU %.1f ms", t->gpuMs);
    gpu_ = text;
}

void FpsOverlay::present(HWND game, bool shown) {
    const ULONGLONG now = GetTickCount64();
    if (lastFrame_) worstMs_ = std::max(worstMs_, double(now - lastFrame_));
    lastFrame_ = now;
    ++frames_;
    if (!since_) since_ = now;
    if (now - since_ < 500) return;
    const double seconds = double(now - since_) / 1000.0;
    const double fps = frames_ / seconds;
    wchar_t generated[64] = L"";
    if (shown_ > 1.05)  // 2.5.24: the frames on the screen, the generated ones too
        std::swprintf(generated, 64, L" (%.0f with frame generation)", fps * shown_);
    wchar_t text[256];
    std::swprintf(text, 256, L"%.0f fps%ls  ·  %.1f ms  (%.0f max)%ls  ·  %hs", fps, generated,
                  fps > 0 ? 1000.0 / fps : 0.0, worstMs_, gpu_.c_str(), label_.c_str());
    frames_ = 0;
    worstMs_ = 0;
    since_ = now;
    const bool show = (shown || !note_.empty()) && game && IsWindowVisible(game) && GetForegroundWindow() == game;
    if (!show) {
        if (window_) ShowWindow(window_, SW_HIDE);
        return;
    }
    draw(game, note_.empty() ? std::wstring(text) : shown ? note_ + L"  ·  " + text : note_);
}

void FpsOverlay::hide() {
    if (window_ && IsWindowVisible(window_)) ShowWindow(window_, SW_HIDE);
    since_ = 0;  // counted anew when it presents again
    frames_ = 0;
    lastFrame_ = 0;
    worstMs_ = 0;
}

void FpsOverlay::draw(HWND game, const std::wstring& text) {
    if (!window_ && windowClass())
        window_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, kClass, L"",
            WS_POPUP, 0, 0, 1, 1, game, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window_) return;
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
        SIZE extent{w, h};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(window_, screen, &pos, &extent, dc, &origin, 0, &blend, ULW_ALPHA);
        if (!IsWindowVisible(window_)) ShowWindow(window_, SW_SHOWNOACTIVATE);
        SelectObject(dc, oldBmp);
    }
    if (bmp) DeleteObject(bmp);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

}  // namespace uagfx::win
