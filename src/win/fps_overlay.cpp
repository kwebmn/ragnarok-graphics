#include "win/fps_overlay.hpp"

#include <algorithm>
#include <cstdio>

#include "win/text_window.hpp"

namespace uagfx::win {
namespace {

const wchar_t kClass[] = L"uaroGraphicsFps";

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
    drawTextWindow(window_, kClass, game, text, TextPlace::TopLeft);
}

}  // namespace uagfx::win
