#pragma once
#include <string>

namespace uagfx::startfail {

// 3.0.17-beta.1 (the concept «простой старт»: «если вышло обновление и оно получилось поломанное»): the installed
// version's failed starts - uaro-graphics\start-failures.txt, "<version> <count>" - so the settings app offers the
// rollback after kOffer of them. The watcher (app/report_send) notes each game's end.

constexpr int kOffer = 2;
constexpr int kEarlyMinutes = 3;  // a crash this soon counts as a failed start

// A game's end as a failed start: the renderer did not start, or a crash in its first minutes
bool failed(const std::string& reason, int minutes);
// The file's text after a game of `version` ended: a failure adds one (another version starts anew), a good end
// forgets them
std::string note(const std::string& text, const std::string& version, bool failedNow);
// Whether to offer the rollback to `version`'s player
bool offerRollback(const std::string& text, const std::string& version);

}  // namespace uagfx::startfail
