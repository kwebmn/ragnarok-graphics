// 3.0.11-beta.180 (the login concept: the site account in the client): the site's client API (core/site_api) - the
// requests as JSON (the token's header only where it is asked), the answers read: a token, the fields refused, the
// code's wait, the game logins and their characters, a login's name free
#include <string>

#include "core/json.hpp"
#include "core/site_api.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(site_api_requests_are_json) {
    const siteapi::Request a = siteapi::authPassword("me@x.ua", "pa\"ss", "uaRO · PC");
    CHECK(a.method == "POST" && a.path == "/api/client/auth" && !a.auth);
    json::Value v;
    std::string error;
    CHECK(json::parse(a.body, v, error));
    CHECK(v["email"].asString() == "me@x.ua" && v["password"].asString() == "pa\"ss");
    CHECK(v["device_name"].asString() == "uaRO · PC" && !v.has("code"));
    CHECK(json::parse(siteapi::authPassword("me@x.ua", "p", "d", "123456").body, v, error) &&
          v["code"].asString() == "123456");
    const siteapi::Request acc = siteapi::accounts();
    CHECK(acc.method == "GET" && acc.path == "/api/client/accounts" && acc.auth && acc.body.empty());
    CHECK(siteapi::devicePoll("abc").path == "/api/client/device/poll" && !siteapi::devicePoll("abc").auth);
    CHECK(siteapi::changePassword(7, "a", "b").path == "/api/client/accounts/7/password");
    CHECK(siteapi::loginFree("ab cd").path == "/api/client/login-free?login=ab%20cd");
    const siteapi::Request r = siteapi::registerAccount("kweb", "secret12", "me@x.ua", 'F', "uaRO · PC", "ru");
    CHECK(json::parse(r.body, v, error) && v["sex"].asString() == "F" && v["agree"].asBool(false));
    CHECK(v["game_userid"].asString() == "kweb" && v["lang"].asString() == "ru" && r.path == "/api/client/register");
}

TEST(site_api_parses_answers) {
    siteapi::TokenAnswer t = siteapi::parseToken(200, R"({"token":"T0K","user":{"nick":"Raphael","email":"e"}})");
    CHECK(t.ok && t.token == "T0K" && t.nick == "Raphael");
    t = siteapi::parseToken(422, R"({"message":"x","errors":{"game_userid":["taken"],"email":["bad","worse"]}})");
    CHECK(!t.ok && t.fieldErrors["game_userid"] == "taken" && t.fieldErrors["email"] == "bad" && !t.twofa);
    t = siteapi::parseToken(422, R"({"twofa":true,"errors":{"code":["need"]}})");
    CHECK(!t.ok && t.twofa);
    t = siteapi::parseToken(500, "<html>");
    CHECK(!t.ok && t.fieldErrors.empty());
    const siteapi::DeviceStart d = siteapi::parseDeviceStart(
        200, R"({"device_code":"dc","user_code":"ABCD-EFGH",)"
             R"("verify_url":"https://uaro.kiev.ua/account/device?code=ABCD-EFGH","interval":5,"expires_in":600})");
    CHECK(d.ok && d.userCode == "ABCD-EFGH" && d.deviceCode == "dc" && d.interval == 5 && d.expiresIn == 600);
    siteapi::TokenAnswer p;
    CHECK(siteapi::parsePoll(428, R"({"error":"authorization_pending"})", p) == siteapi::Poll::Pending);
    CHECK(siteapi::parsePoll(410, R"({"error":"expired_token"})", p) == siteapi::Poll::Expired);
    CHECK(siteapi::parsePoll(200, R"({"token":"X","user":{"nick":"n"}})", p) == siteapi::Poll::Approved &&
          p.token == "X");
    CHECK(siteapi::parsePoll(0, "", p) == siteapi::Poll::Failed);
    const std::vector<siteapi::SiteLogin> logins = siteapi::parseAccounts(200, R"({"logins":[)"
        R"({"account_id":2000001,"userid":"kweb","primary":true,"banned":false,"ban_until":0,"chars":[)"
        R"({"name":"Raphael","class":4009,"job":"High Priest","base_level":99,"last_map":"prontera",)"
        R"("char_num":2,"online":false},)"
        R"({"name":"Lavka","class":4019,"job":"Creator","base_level":70,"last_map":"alberta","char_num":0,)"
        R"("online":true}]},)"
        R"({"account_id":2000002,"userid":"kweb_alt","primary":false,"banned":true,"ban_until":1762200000,"chars":[)"
        R"({"name":"Ezzy","class":4013,"job":"Assassin Cross","base_level":92,"last_map":"morocc","char_num":1}]}]})");
    CHECK(logins.size() == 2 && logins[0].chars.size() == 2 && logins[1].chars.size() == 1);
    CHECK(logins[0].accountId == 2000001 && logins[0].login == "kweb" && logins[0].primary);
    CHECK(logins[0].chars[0].slot == 2 && logins[0].chars[0].jobId == 4009 && logins[0].chars[0].level == 99);
    CHECK(logins[0].chars[1].online && logins[1].banned && logins[1].banUntil == 1762200000);
    CHECK(siteapi::parseAccounts(401, R"({"message":"Unauthenticated."})").empty());
    CHECK(siteapi::parseFree(200, R"({"free":true})") == 1 && siteapi::parseFree(200, R"({"free":false})") == 0);
    CHECK(siteapi::parseFree(422, "{}") == -1);
}
