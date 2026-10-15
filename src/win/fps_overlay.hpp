#pragma once
#include <windows.h>

#include <string>

namespace uagfx::win {

// 0.54: the frame counter - "143 fps · 7.0 ms · Direct3D 11" in the game window's top left corner, in a small
// layered window of its own over it (no draw of the renderer's: the same with Direct3D 12, 11 and the 64-bit
// renderer). Ctrl+Shift+F11 shows and hides it; [display] fps_counter = 1 shows it from the start.
class FpsOverlay {
public:
    ~FpsOverlay();
    void setLabel(const std::string& api) { label_ = api; }
    // Every present of the game: counts, and twice a second redraws (or hides it, the game not in front).
    void present(HWND game, bool shown);

private:
    void draw(HWND game, const std::wstring& text);
    HWND window_ = nullptr;
    std::string label_;
    ULONGLONG since_ = 0;
    unsigned frames_ = 0;
    ULONGLONG lastFrame_ = 0;
    double worstMs_ = 0;
    std::wstring text_;
};

}  // namespace uagfx::win
