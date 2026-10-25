#pragma once
#include <string>
#include <vector>

namespace uagfx::reports {

// 2.13: «Звернення» - a tester's reports, their statuses and the developer's answers, from uaro.kiev.ua (the site's
// GraphicsReportController): GET /api/graphics/reports/mine?computer=K, GET /api/graphics/reports/{code}?computer=K.
// No login: the reports belong to the computer's random key (uaro-graphics\reports\computer.txt).
enum class Status { New, Read, NeedInfo, Fixing, Fixed, Closed };
// None: an automatic report, or one from before 2.13; 3.0.5-beta.6: Test - «Тест графіки»'s (never the player's pick)
enum class Kind { None, Graphics, Crash, Slow, Other, Test };

struct Summary {
    std::string code, title, reason, fixedIn, at, updated;  // at / updated: ISO 8601 as the site writes them
    Status status = Status::New;
    Kind kind = Kind::None;
    bool automatic = false;
    int unread = 0;  // the developer's messages not seen yet
};
struct Message {
    bool fromDev = false;
    std::string text, at;
    std::vector<std::string> shots;  // plain file names: /api/graphics/reports/{code}/shots/{name}
    bool hasStatus = false;          // this message changed the status (text may then be empty)
    Status status = Status::New;
};
struct Conversation {
    std::string code, reason, fixedIn, version, at;
    Status status = Status::New;
    Kind kind = Kind::None;
    bool automatic = false;
    std::vector<Message> messages;
};

// false and why for anything that is not that answer (an error page, the limit's "Too Many Attempts").
bool parseList(const std::string& json, std::vector<Summary>& out, std::string& error);
bool parseConversation(const std::string& json, Conversation& out, std::string& error);

const char* kindToken(Kind k);  // "graphics", "crash", "slow", "other"; "" for None
const char* statusToken(Status s);
Status statusOf(const std::string& token);  // New for what it does not know
int unreadOf(const std::vector<Summary>& list);
// «Підтверджую» / «Ще є» once the fix is here: «Виправлено в X» and this install at X or later.
bool canConfirm(const Conversation& c, const std::string& installedVersion);

// The site writes its times in UTC with an offset; the app shows them in the player's clock.
struct Civil {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
};
bool utcOf(const std::string& iso, Civil& out);  // "2026-10-22T18:57:32+03:00" -> 15:57:32 that day
std::string shown(const Civil& local);           // "22.10.2026 21:07"

}  // namespace uagfx::reports
