#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/reports.hpp"

namespace uagfx::answers {

// 3.0.9-beta.4 (the user: «делай все» - the answers where the player is): uaro-graphics\reports\answers.json
// (layout::kAnswers) - the developer's answers not seen yet, newest first, as the game's watcher last heard them from
// the site (at the game's start, then every 10 minutes while it runs); the driver shows the newest in the game (a card)
// or, the game behind, in a Windows notification.
std::string write(const std::vector<reports::Answer>& list);
// false: not such a file (half written, another one) - the reader keeps what it had. Answers without a plain code are
// left out: the code goes to a command line (uaro-graphics.exe --answer R-XXXXXX).
bool read(const std::string& json, std::vector<reports::Answer>& out);
// What tells answers apart (a card hidden by «Пізніше» stays hidden for that answer): "R-XXXXXX#id", from a site
// without ids "R-XXXXXX@time"
std::string key(const reports::Answer& a);
bool plainCode(const std::string& code);  // "R-" and six of A-Z, 0-9: the site's own
// 3.0.10-beta.3 (the owner: «сделай» - the app, minimised, asks the site every 5 minutes): the answer its Windows
// notification tells of after a check - the newest one newer than `known` (the newest of the checks before; -1: no
// check yet - the start's tells of nothing, its strip does), when the player wants such notifications, the game is not
// running (its own watcher tells then) and the app is not in front (its strip says it). Answers without an id (a site
// before 3.0.9-beta.4) are never newer.
std::optional<reports::Answer> toAnnounce(const std::vector<reports::Answer>& list, long long known, bool wanted,
                                          bool gameRunning, bool appInFront);
long long newestId(const std::vector<reports::Answer>& list);  // 0: none

// 3.0.20-beta.17 (the owner: «чтобы не задалбывало постоянно вверху сообщение в игре что надо ответить» - the card
// came again for every answer and every game): the game's one card for the answers newer than the ones put off
// (`putOff`: the newest id put off - by «Пізніше», by opening, by the card gone by itself; answers without an id only
// while none is): the one the developer waits on (need_info) shown first, else the newest, with how many more; it
// waits for the player only while the developer waits for an answer, else it goes after kCardShowsMs; `newestId` -
// what putting it off puts off
struct Card {
    const reports::Answer* shown = nullptr;  // none: no card
    size_t more = 0;
    bool waits = false;
    long long newestId = 0;
};
constexpr unsigned kCardShowsMs = 12000;
Card cardOf(const std::vector<reports::Answer>& list, long long putOff);
// uaro-graphics\reports\answers-later.txt (layout::kAnswersLater): the id put off, kept between games; 0 none
long long putOffOf(const std::string& text);
std::string putOffText(long long id);

}  // namespace uagfx::answers
