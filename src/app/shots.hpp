#pragma once
#include <windows.h>

#include <string>
#include <utility>
#include <vector>

namespace uagfx::app {

// 2.13: «Нове звернення» - the game's own screenshots to tick: its ScreenShot folder, the last 7 days, newest first
// (Windows' Screenshots folder only through «Додати файл…»).
std::vector<std::wstring> gameShots(const std::wstring& gameDir, size_t most);
// A picture (JPG, PNG, BMP) as a report's JPEG: at most 1600 pixels across and 600 KB; "" when Windows cannot read it.
std::string shotJpeg(const std::wstring& path);
// The clipboard's picture (a copied screenshot) as such a JPEG; "" when it holds none.
std::string clipboardJpeg(HWND owner);
// A picture file or a JPEG's bytes as a w x h bitmap for a list, fitted inside on grey; nullptr when unreadable.
HBITMAP shotThumb(const std::wstring& path, int w, int h);
HBITMAP jpegThumb(const std::string& jpeg, int w, int h);

}  // namespace uagfx::app
