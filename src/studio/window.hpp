#pragma once
#include <windows.h>

#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::studio {

constexpr const wchar_t* kWindowClass = L"uaroStudio";  // a second studio finds the first by it

// 2.3, uaRO Studio's main window (the spec's "The window"): its tabs over one jobs bar.
class StudioWindow {
public:
    StudioWindow(std::wstring gameDir, settings::Lang lang) : gameDir_(std::move(gameDir)), lang_(lang) {}
    bool create(HINSTANCE instance, int show);
    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    std::wstring t(settings::Text x) const;

    std::wstring gameDir_;
    settings::Lang lang_;
    HWND hwnd_ = nullptr;
};

}  // namespace uagfx::studio
