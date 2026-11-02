#include "win/site_accounts.hpp"

#include <windows.h>

#include "win/log.hpp"
#include "win/site_http.hpp"
#include "win/site_key.hpp"

namespace uagfx::win::siteaccounts {
namespace {

constexpr ULONGLONG kAgainMs = 2 * 60 * 1000;

struct State {
    std::vector<siteapi::SiteLogin> logins;
    bool asking = false;
    ULONGLONG askedAt = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

const std::vector<siteapi::SiteLogin>& logins() {
    State& s = state();
    if (sitekey::token().empty()) s.logins.clear();  // signed out (here or in another game window)
    return s.logins;
}

void refresh(bool now) {
    State& s = state();
    if (sitekey::token().empty() || s.asking) return;
    const ULONGLONG tick = GetTickCount64();
    if (!now && s.askedAt && tick - s.askedAt < kAgainMs) return;
    s.asking = true;
    s.askedAt = tick;
    sitehttp::send(siteapi::accounts(), [](int status, const std::string& body) {
        State& st = state();
        st.asking = false;
        if (status == 401) {
            sitekey::forget();
            st.logins.clear();
            LOG_INFO("site: the device's token is no longer valid (revoked in the cabinet?) - signed out");
            return;
        }
        if (status != 200) return;  // kept as they were
        st.logins = siteapi::parseAccounts(status, body);
        LOG_INFO("site: %zu game logins of the site account", st.logins.size());
    });
}

}  // namespace uagfx::win::siteaccounts
