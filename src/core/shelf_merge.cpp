#include "core/shelf_merge.hpp"

namespace uagfx::shelfmerge {

std::vector<Row> merge(const accounts::Shelf& local, const std::vector<siteapi::SiteLogin>& site) {
    std::vector<Row> rows;
    std::vector<bool> taken(site.size(), false);
    const auto fill = [&](Row& r, size_t i) {
        const siteapi::SiteLogin& l = site[i];
        taken[i] = true;
        r.fromSite = true;
        r.accountId = l.accountId;
        r.banned = l.banned;
        r.banUntil = l.banUntil;
        r.chars = l.chars;
    };
    for (size_t i = 0; i < local.list.size(); ++i) {
        const accounts::Account& a = local.list[i];
        Row r;
        r.login = a.login;
        r.label = a.label;
        r.hasPassword = !a.password.empty() && !a.refused;
        r.local = int(i);
        const uint64_t h = accounts::loginHash(a.login);
        for (size_t j = 0; j < site.size(); ++j)
            if (!taken[j] && accounts::loginHash(site[j].login) == h) {
                fill(r, j);
                break;
            }
        rows.push_back(r);
    }
    for (const bool primary : {true, false})
        for (size_t j = 0; j < site.size(); ++j)
            if (!taken[j] && site[j].primary == primary) {
                Row r;
                r.login = site[j].login;
                fill(r, j);
                rows.push_back(r);
            }
    return rows;
}

}  // namespace uagfx::shelfmerge
