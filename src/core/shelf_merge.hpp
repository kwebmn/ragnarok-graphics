#pragma once
#include <string>
#include <vector>

#include "core/accounts.hpp"
#include "core/site_api.hpp"

namespace uagfx::shelfmerge {

// 3.0.11-beta.181 (the login concept: the site account in the client): the shelf with the site's logins - this
// computer's accounts in their order first (their passwords kept here), the site's other logins after them (the
// primary first; their passwords asked once, then kept), a login the same in any case. Signed out: the computer's own.
struct Row {
    std::string login, label;
    bool hasPassword = false, fromSite = false, banned = false;
    long long banUntil = 0;
    int accountId = 0;  // the site's (0: not the site's)
    int local = -1;     // its place on this computer's shelf, -1 none
    std::vector<siteapi::SiteChar> chars;
};
std::vector<Row> merge(const accounts::Shelf& local, const std::vector<siteapi::SiteLogin>& site);

}  // namespace uagfx::shelfmerge
