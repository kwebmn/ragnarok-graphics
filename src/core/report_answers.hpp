#pragma once
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

}  // namespace uagfx::answers
