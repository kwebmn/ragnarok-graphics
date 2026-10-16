#pragma once
#include <windows.h>

#include <string>
#include <utility>
#include <vector>

namespace uagfx::app {

// 1.6.2: the AI page's preview in a window of its own - the pictures side by side in their own proportions, the
// wheel zooming all of them about the point under the mouse, a drag moving all of them, a double click back to the
// whole; zoomed in, pixel by pixel. One window: opened again, it shows the new pictures. hint: the title's line.
void showPreviewWindow(HWND owner, const std::vector<std::pair<std::wstring, std::wstring>>& files,
                       const std::wstring& title);

}  // namespace uagfx::app
