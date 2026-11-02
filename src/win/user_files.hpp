#pragma once
#include <string>

namespace uagfx::win::userfiles {

// 3.0.11-beta.180: this Windows user's own files of uaRO Graphics, any game folder - %LOCALAPPDATA%\uaroGraphics
// (the login screen's shelf, the site account's token), their words DPAPI-protected (win/dpapi); wide paths (a Cyrillic
// user name), written through a temporary file. `name` a file's name there ("accounts.dat").
std::wstring path(const char* name);  // "" when %LOCALAPPDATA% is not known
unsigned long long writeTime(const char* name);  // 0: none
bool readSealed(const char* name, std::string& plain);  // false: none, or not readable (another user's, damaged)
bool writeSealed(const char* name, const std::string& plain);
bool remove(const char* name);   // true: gone (or never there)
bool keepAside(const char* name);  // renamed to name + ".bad" (replacing an older one)

}  // namespace uagfx::win::userfiles
