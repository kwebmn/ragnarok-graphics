#pragma once
#include <vector>

#include "core/site_api.hpp"

namespace uagfx::win::siteaccounts {

// 3.0.11-beta.181 (the login concept: the site account in the client): the site account's game logins and their
// characters (GET /api/client/accounts) for the shelf - asked while the login screen shows and the site account is
// signed in (at once after a sign-in or a form, then every 2 minutes); 401: the token revoked in the cabinet - signed
// out here. Empty when signed out. On the game's thread.
const std::vector<siteapi::SiteLogin>& logins();
void refresh(bool now);  // the login screen's frame: ask when due (now: at once)

}  // namespace uagfx::win::siteaccounts
