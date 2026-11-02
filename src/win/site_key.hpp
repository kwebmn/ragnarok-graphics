#pragma once
#include <string>

namespace uagfx::win::sitekey {

// 3.0.11-beta.180 (the login concept: the site account in the client): the site account's device token on this
// computer for this Windows user - %LOCALAPPDATA%\uaroGraphics\site.dat (DPAPI, win/user_files): the token and the
// site's nick; never in a log. On the game's thread.
std::string token();  // "" none
std::string nick();
bool save(const std::string& token, const std::string& nick);
void forget();
std::string deviceName();  // «uaRO · <this computer's name>» (the cabinet's «Пристрої» list)

}  // namespace uagfx::win::sitekey
