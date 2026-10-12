#pragma once
#include <windows.h>

#include <string>

namespace uagfx::win {

std::string gameDir();               // folder of the game's exe, no trailing backslash
std::string systemDir();             // System32; SysWOW64 for our 32-bit process on 64-bit Windows
std::string modulePath(HMODULE m);   // "" when m is null
bool fileExists(const std::string& path);
bool readTextFile(const std::string& path, std::string& out);

}  // namespace uagfx::win
