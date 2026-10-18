#pragma once
#include <windows.h>

#include <string>
#include <vector>

#include "core/settings/ini_edit.hpp"

namespace uagfx::win {

std::string gameDir();               // folder of the game's exe, no trailing backslash
std::string systemDir();             // System32; SysWOW64 for our 32-bit process on 64-bit Windows
std::string modulePath(HMODULE m);   // "" when m is null
bool fileExists(const std::string& path);
// 2.3: a file's size and last write time (Unix seconds) - uaRO Studio's catalog knows the game's archives by them
bool fileStamp(const std::string& path, uint64_t& size, int64_t& time);
bool readTextFile(const std::string& path, std::string& out);
// 2.2.2: a file written whole into one of this process's own, then put in its place in one step (MoveFileEx): a
// reader meanwhile gets the old file or the new one, never a part. false: not written, the old one stays.
bool writeFileAtomic(const std::string& path, const void* data, size_t size);
bool writeFileAtomic(const std::string& path, const std::string& text);
// 2.2.2: uaro-graphics.ini edited (core/settings/ini_edit.hpp) and written whole - read again when it changed while
// it was edited (the settings app wrote it meanwhile), up to three times. The console's and Ctrl+Shift+F11's.
bool editIniFile(const std::string& path, const std::vector<settings::IniEdit>& edits);

}  // namespace uagfx::win
