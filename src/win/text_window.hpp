#pragma once
#include <windows.h>

#include <string>

namespace uagfx::win {

// A line of text over the game window in a small layered window of its own - white on a half-dark band, sized by the
// monitor's DPI: the frame counter in the top left corner (0.54), 2.6.17 the map entry's countdown in the top middle.
// `window` is made on the first call (`windowClass` its class's name) and kept by the caller.
enum class TextPlace { TopLeft, TopMiddle };
void drawTextWindow(HWND& window, const wchar_t* windowClass, HWND game, const std::wstring& text, TextPlace place);

}  // namespace uagfx::win
