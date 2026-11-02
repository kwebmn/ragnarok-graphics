#pragma once
#include <functional>
#include <string>

#include "core/site_api.hpp"

namespace uagfx::win::sitehttp {

// 3.0.11-beta.180 (the login concept: the site account in the client): uaro.kiev.ua's client API over HTTPS
// (core/site_api) - winhttp.dll asked for only at the first request (the driver's imports stay the four), a thread a
// request (15 s at most), the answers run on the game's thread from pump() (each frame). The device token
// (win/site_key) in the Authorization header where the request asks. Logs the path and the status only.
using Answer = std::function<void(int status, const std::string& body)>;  // status 0: the site not reached
// `token`: in place of win/site_key's (the cabinet's key tried before it is kept)
void send(const siteapi::Request& r, Answer answer, const std::string& token = std::string());
void pump();
bool busy();  // a request on its way

}  // namespace uagfx::win::sitehttp
