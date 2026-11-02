// 3.0.11-beta.181 (the login concept: the site account in the client): the shelf with the site's logins
// (core/shelf_merge) - this computer's accounts in their order first (their passwords kept), the site's other logins
// after them (the primary first, no password: asked once), a login the same in any case
#include <string>
#include <vector>

#include "core/shelf_merge.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(shelf_merge_local_and_site) {
    accounts::Shelf local;
    accounts::upsert(local, "kweb", "pw", 1);
    accounts::upsert(local, "old_one", "pw2", 2);  // not the site's: stays
    local.list[0].label = "Основний";
    std::vector<siteapi::SiteLogin> site(3);
    site[0].accountId = 11;
    site[0].login = "KWEB";
    site[0].chars.push_back(siteapi::SiteChar{"Raphael", "High Priest", "prontera", 4009, 99, 2, false});
    site[1].accountId = 12;
    site[1].login = "kweb_alt";
    site[1].banned = true;
    site[1].banUntil = 1762200000;
    site[2].accountId = 13;
    site[2].login = "kweb_main";
    site[2].primary = true;
    const std::vector<shelfmerge::Row> rows = shelfmerge::merge(local, site);
    CHECK(rows.size() == 4);
    CHECK(rows[0].login == "kweb" && rows[0].hasPassword && rows[0].fromSite && rows[0].chars.size() == 1);
    CHECK(rows[0].label == "Основний" && rows[0].accountId == 11 && rows[0].local == 0);
    CHECK(rows[1].login == "old_one" && rows[1].hasPassword && !rows[1].fromSite && rows[1].local == 1);
    CHECK(rows[2].login == "kweb_main" && !rows[2].hasPassword && rows[2].fromSite && rows[2].local == -1);
    CHECK(rows[3].login == "kweb_alt" && rows[3].banned && rows[3].banUntil == 1762200000);
    CHECK(shelfmerge::merge(local, {}).size() == 2);  // signed out: the computer's own alone
}
