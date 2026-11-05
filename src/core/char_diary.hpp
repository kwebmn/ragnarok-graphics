#pragma once
#include <map>
#include <string>

namespace uagfx::chardiary {

// 3.0.11-beta.273 (the owner's features 3 and 4, from nostalRE's hub: «грав 2 дні тому» on a character's card): what
// this computer saw of each character - when it was last in the game, on which map, at which levels, how long it has
// been played here. Kept in uaro-graphics\characters.txt, a line a character: its name (UTF-8) and the numbers, tabs
// between; written by win/char_diary while the character plays.
struct Entry {
    long long lastSeen = 0;  // unix time
    std::string map;
    int level = 0, jobLevel = 0, job = -1;
    long long played = 0;  // seconds in the game on this computer
    // 3.0.11-beta.274 (the owner's feature 4): its last session - how long, the base levels' share gained (per cent of
    // a level, 100 a whole one), the job's, the zeny's change, when it ended
    long long sessionSeconds = 0, zenyGain = 0, sessionEnd = 0;
    int baseGain = 0, jobGain = 0;
};
// The share of levels gained between two points (level, exp, next): 100 a whole level; next 0 the level alone
int gained(int levelFrom, long long expFrom, long long nextFrom, int levelTo, long long expTo, long long nextTo);
using Diary = std::map<std::string, Entry>;

Diary parse(const std::string& text);
std::string text(const Diary& d);

struct SummaryTexts {
    std::string head, hours, minutes, base, job, zeny;  // «Остання гра {1}:», «{1} год {2} хв», «{1} хв», «Base +{1} %»
};
// «Остання гра Лучниця: 1 год 20 хв · Base +134 % · Job +40 % · +12 500 z»; "" no session kept
std::string summary(const std::string& name, const Entry& e, const SummaryTexts& t, char decimal);

struct AgoTexts {
    std::string now, minutes, hours, days, months;  // «щойно», «{1} хв тому», «{1} год тому», «{1} дн. тому», ...
};
// How long ago `then` was by `now` (unix seconds): under 2 minutes «щойно», minutes under an hour, hours under 2 days,
// days under 60, months past; "" for a time not known
std::string ago(long long now, long long then, const AgoTexts& t);

}  // namespace uagfx::chardiary
