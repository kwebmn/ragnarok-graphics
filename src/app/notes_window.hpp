#pragma once
#include <windows.h>

#include <string>

namespace uagfx::app {

// 2.6: a long text ("What's new") in a window of its own over the owner, its lines scrolling; returns once it is
// closed - the owner disabled meanwhile, as under a message box.
void showNotes(HWND owner, const std::wstring& title, const std::wstring& text, HFONT font, int dpi);

}  // namespace uagfx::app
