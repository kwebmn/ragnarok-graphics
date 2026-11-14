#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace uagfx::siteapi {

// 3.0.11-beta.180 (the login concept: the site account in the client): uaro.kiev.ua's client API - a device token
// without cookies (/api/client/*: the site's password, a code in the browser, the registration in one form), the game
// logins and their characters for the shelf. The requests as JSON (win/site_http sends them, the token's header only
// where `auth`); the answers read here. Never a request's or an answer's body in a log: the path and the status only.
struct Request {
    std::string method, path, body;  // body: JSON, "" none
    bool auth = false;               // the device token's Authorization header
};
Request authPassword(const std::string& email, const std::string& password, const std::string& device,
                     const std::string& code = std::string());  // code: the site's two-factor, when it asks
Request deviceStart(const std::string& device);
Request devicePoll(const std::string& deviceCode);
Request accounts();
// 3.0.19-beta.4: the character's quests' steps as the server keeps them (GET /api/client/quests/progress?char=<the name
// as UTF-8, percent-encoded>; only the site account's own characters - core/quest_progress reads the answer)
Request questProgress(const std::string& charName);
Request createLogin(const std::string& login, const std::string& password, char sex);
Request linkLogin(const std::string& login, const std::string& password);
Request changePassword(int accountId, const std::string& current, const std::string& next);
Request loginFree(const std::string& login);
Request registerAccount(const std::string& login, const std::string& password, const std::string& email, char sex,
                        const std::string& device, const std::string& lang);  // lang: ua, ru, en (the site's)

struct TokenAnswer {
    bool ok = false;
    std::string token, nick;
    std::map<std::string, std::string> fieldErrors;  // a refused field: its first message
    bool twofa = false;                              // the site wants its two-factor code too
};
TokenAnswer parseToken(int status, const std::string& body);
struct DeviceStart {
    bool ok = false;
    std::string deviceCode, userCode, verifyUrl;
    int interval = 5, expiresIn = 600;
};
DeviceStart parseDeviceStart(int status, const std::string& body);
enum class Poll : uint8_t { Pending, Approved, Expired, Failed };
Poll parsePoll(int status, const std::string& body, TokenAnswer& out);
struct SiteChar {
    std::string name, job, map;  // UTF-8
    int jobId = 0, level = 0, slot = -1;
    bool online = false;
};
struct SiteLogin {
    int accountId = 0;
    std::string login;
    bool primary = false, banned = false;
    long long banUntil = 0;
    std::vector<SiteChar> chars;
};
std::vector<SiteLogin> parseAccounts(int status, const std::string& body);  // empty: none or not the answer
int parseFree(int status, const std::string& body);                         // 1 free, 0 taken, -1 not known

}  // namespace uagfx::siteapi
