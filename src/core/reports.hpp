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

// 3.0.9-beta.4: an answer of the developer's not seen yet - what the game's card, the Windows notification and the
// launcher's strip tell of (answersOf; uaro-graphics\reports\answers.json, core/report_answers)
struct Answer {
    long long id = 0;                     // the site's message; 0 from a site without "answer" (before 3.0.9-beta.4)
    std::string code, text, at, fixedIn;  // text: its start (300 characters at most), "" when none came
    Status status = Status::New;          // the report's now
};

struct Summary {
    std::string code, title, reason, fixedIn, at, updated;  // at / updated: ISO 8601 as the site writes them
    Status status = Status::New;
    Kind kind = Kind::None;
    bool automatic = false;
    int unread = 0;  // the developer's messages not seen yet
    // 3.0.9-beta.4: the newest of them (the site's "answer"), and the last message's author ("player", "dev"; "" none),
    // start and time - the list's second line
    bool hasAnswer = false;
    Answer answer;
    std::string lastAuthor, lastText, lastAt;
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
// 3.0.9-beta.4: the unread reports' answers, newest first (a site without "answer": one per unread report, no words,
// its update time)
std::vector<Answer> answersOf(const std::vector<Summary>& list);
// «Підтверджую» / «Ще є» once the fix is here: «Виправлено в X» and this install at X or later.
bool canConfirm(const Conversation& c, const std::string& installedVersion);

// The site writes its times in UTC with an offset; the app shows them in the player's clock.
struct Civil {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
};
bool utcOf(const std::string& iso, Civil& out);  // "2026-10-22T18:57:32+03:00" -> 15:57:32 that day
std::string shown(const Civil& local);           // "22.10.2026 21:07"

}  // namespace uagfx::reports
