#pragma once
#include <windows.h>

#include <string>

#include "core/gpu_timing.hpp"

namespace uagfx::win {

// 0.54: the frame counter - "143 fps · 7.0 ms · Direct3D 11" in the game window's top left corner, in a small
// layered window of its own over it (no draw of the renderer's: the same with Direct3D 12, 11 and the 64-bit
// renderer). Ctrl+Shift+F11 shows and hides it; [frames] fps_counter = 1 shows it (2.2.1: at once, and the key
// writes it). 2.2.1: and the card's work a frame - "GPU 7.1 ms (DLSS Quality 2.6)": at the frame limit the frames a
// second stay the same whatever is on, the GPU's milliseconds show what an effect costs. 2.5.24: with frame
// generation also the frames shown - "42 fps (166 with frame generation)".
class FpsOverlay {
public:
    ~FpsOverlay();
    void setLabel(const std::string& api) { label_ = api; }
    void setGpu(const GpuTiming* t);  // the last measurement, nullptr: none
    // 2.15: a line shown whether the counter is or not (Pause kept the improvements off); "" none
    void setNote(const std::wstring& note) { note_ = note; }
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
    std::wstring gpu_;
    double shown_ = 0;  // 2.5.24: frames shown a frame of the game with frame generation; 0 none
    std::wstring note_;
};

}  // namespace uagfx::win
