#pragma once
#include <string>
#include <utility>
#include <vector>

namespace uagfx::app {

// 1.8.1: screenshots for a report - the newest pictures (JPG, PNG, BMP) of the last day from the folders (the game's
// ScreenShot, Windows' Screenshots), at most `most`, each made a JPEG of at most 1600 pixels across (Windows' own
// codecs): name -> its bytes.
std::vector<std::pair<std::string, std::string>> recentShots(const std::vector<std::wstring>& folders, size_t most);

// The folders looked in: the game's ScreenShot and Pictures\Screenshots.
std::vector<std::wstring> shotFolders(const std::wstring& gameDir);

}  // namespace uagfx::app
