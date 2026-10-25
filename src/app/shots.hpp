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
// 3.0.5-beta.5 (the user: «сделай так чтобы снимки делались без уменьшения качества»): the same pictures in full -
// a PNG of their own size (the game's own shots are PNGs already: their bytes as they are), sent after the report in
// parts (app/reports sendOriginal); "" when unreadable or over the site's kMostOriginal
constexpr size_t kMostOriginal = 40'000'000;  // the site's MAX_ORIGINAL
std::string shotPng(const std::wstring& path);
std::string clipboardPng(HWND owner);
// A picture file or a JPEG's bytes as a w x h bitmap for a list, fitted inside on grey; nullptr when unreadable.
HBITMAP shotThumb(const std::wstring& path, int w, int h);
HBITMAP jpegThumb(const std::string& jpeg, int w, int h);

}  // namespace uagfx::app
