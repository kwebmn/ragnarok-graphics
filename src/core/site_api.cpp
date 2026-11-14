#include "core/site_api.hpp"

#include <cstdint>

#include "core/json.hpp"

namespace uagfx::siteapi {
namespace {

Request post(const std::string& path, const json::Value& body, bool auth) {
    return Request{"POST", path, json::dump(body), auth};
}

json::Value object(std::initializer_list<std::pair<const char*, json::Value>> members) {
    json::Value v = json::Value::object();
    for (const auto& m : members) v.set(m.first, m.second);
    return v;
}

std::string escaped(const std::string& s) {  // a query's value
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~')
            out += char(c);
        else
            out += std::string("%") + hex[c >> 4] + hex[c & 15];
    }
    return out;
}

bool parsed(const std::string& body, json::Value& v) {
    std::string error;
    return json::parse(body, v, error) && v.type() == json::Value::Object;
}

}  // namespace

Request authPassword(const std::string& email, const std::string& password, const std::string& device,
                     const std::string& code) {
    json::Value v = object({{"email", email}, {"password", password}, {"device_name", device}});
    if (!code.empty()) v.set("code", json::Value(code));
    return post("/api/client/auth", v, false);
}

Request deviceStart(const std::string& device) {
    return post("/api/client/device", object({{"device_name", device}}), false);
}

Request devicePoll(const std::string& deviceCode) {
    return post("/api/client/device/poll", object({{"device_code", deviceCode}}), false);
}

Request accounts() { return Request{"GET", "/api/client/accounts", "", true}; }

Request questProgress(const std::string& charName) {
    static const char* const kHex = "0123456789ABCDEF";
    std::string q;
    for (const char c : charName) {  // RFC 3986's unreserved as they are, every other byte %XX
        const unsigned char u = static_cast<unsigned char>(c);
        if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || u == '-' || u == '_' ||
            u == '.' || u == '~')
            q += c;
        else
            q += {'%', kHex[u >> 4], kHex[u & 15]};
    }
    return Request{"GET", "/api/client/quests/progress?char=" + q, "", true};
}

Request createLogin(const std::string& login, const std::string& password, char sex) {
    return post("/api/client/accounts",
                object({{"game_userid", login}, {"game_password", password}, {"sex", std::string(1, sex)}}), true);
}

Request linkLogin(const std::string& login, const std::string& password) {
    return post("/api/client/accounts/link", object({{"game_userid", login}, {"game_password", password}}), true);
}

Request changePassword(int accountId, const std::string& current, const std::string& next) {
    return post("/api/client/accounts/" + std::to_string(accountId) + "/password",
                object({{"current_password", current}, {"password", next}}), true);
}

Request loginFree(const std::string& login) {
    return Request{"GET", "/api/client/login-free?login=" + escaped(login), "", false};
}

Request registerAccount(const std::string& login, const std::string& password, const std::string& email, char sex,
                        const std::string& device, const std::string& lang) {
    return post("/api/client/register",
                object({{"game_userid", login}, {"password", password}, {"email", email},
                        {"sex", std::string(1, sex)}, {"agree", true}, {"device_name", device}, {"lang", lang}}),
                false);
}

TokenAnswer parseToken(int status, const std::string& body) {
    TokenAnswer t;
    json::Value v;
    if (!parsed(body, v)) return t;
    if ((status == 200 || status == 201) && !v["token"].asString().empty()) {
        t.ok = true;
        t.token = v["token"].asString();
        t.nick = v["user"]["nick"].asString();
        return t;
    }
    t.twofa = v["twofa"].asBool(false);
    for (const auto& [field, messages] : v["errors"].members()) {
        const std::vector<json::Value>& list = messages.items();
        t.fieldErrors[field] = list.empty() ? messages.asString() : list.front().asString();
    }
    return t;
}

DeviceStart parseDeviceStart(int status, const std::string& body) {
    DeviceStart d;
    json::Value v;
    if (status != 200 || !parsed(body, v)) return d;
    d.deviceCode = v["device_code"].asString();
    d.userCode = v["user_code"].asString();
    d.verifyUrl = v["verify_url"].asString();
    d.interval = int(v["interval"].asInt(5));
    d.expiresIn = int(v["expires_in"].asInt(600));
    d.ok = !d.deviceCode.empty() && !d.userCode.empty();
    return d;
}

Poll parsePoll(int status, const std::string& body, TokenAnswer& out) {
    if (status == 428) return Poll::Pending;
    if (status == 410) return Poll::Expired;
    if (status != 200) return Poll::Failed;
    out = parseToken(status, body);
    return out.ok ? Poll::Approved : Poll::Failed;
}

std::vector<SiteLogin> parseAccounts(int status, const std::string& body) {
    std::vector<SiteLogin> out;
    json::Value v;
    if (status != 200 || !parsed(body, v)) return out;
    for (const json::Value& l : v["logins"].items()) {
        SiteLogin s;
        s.accountId = int(l["account_id"].asInt(0));
        s.login = l["userid"].asString();
        s.primary = l["primary"].asBool(false);
        s.banned = l["banned"].asBool(false);
        s.banUntil = l["ban_until"].asInt(0);
        for (const json::Value& c : l["chars"].items()) {
            SiteChar ch;
            ch.name = c["name"].asString();
            ch.job = c["job"].asString();
            ch.map = c["last_map"].asString();
            ch.jobId = int(c["class"].asInt(0));
            ch.level = int(c["base_level"].asInt(0));
            ch.slot = int(c["char_num"].asInt(-1));
            ch.online = c["online"].asBool(false);
            s.chars.push_back(ch);
        }
        if (s.accountId > 0 && !s.login.empty()) out.push_back(s);
    }
    return out;
}

int parseFree(int status, const std::string& body) {
    json::Value v;
    if (status != 200 || !parsed(body, v) || v["free"].type() != json::Value::Bool) return -1;
    return v["free"].asBool(false) ? 1 : 0;
}

}  // namespace uagfx::siteapi
