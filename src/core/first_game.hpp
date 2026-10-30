#pragma once
#include <string>

namespace uagfx::firstgame {

// 3.0.11-beta.61 (the autopilot, the owner: «делай»): how the first game on this install went - written by the driver
// (layout::kFirstGame: at the start, then at its first minute on the maps), finished by the settings app's watcher when
// that game closes, sent once with the anonymous install ping (core/install_ping) - a new player's first start seen:
// did it get to a map, at what frames, did it close by an error. Nothing of the player in it.
struct Outcome {
    std::string version;  // the driver's
    std::string cls;      // "L<the card's memory level> <api>": "L1 d3d11"
    bool safe = false;    // started in the safe mode
    bool map = false;     // got to a map (a minute on the maps)
    int fps = -1;         // those windows' median frames; -1 not known
    int level = -1;       // the automatic set's level then; -1 not known
    std::string end;      // "" not known yet; "fine", "crash", "start-failed", "gpu-lost"
    int minutes = -1;     // the game's length
};
std::string format(const Outcome& o);                 // "key=value" lines
bool parse(const std::string& text, Outcome& out);  // false without a version
bool complete(const Outcome& o);                    // its end known
std::string endOfLog(const std::string& log);       // report::reasonOfLog, "fine" without one, "" for no log
int minutesOfLog(const std::string& log);           // its first to its last line by their times; -1 none
// {"class":"L1 d3d11","safe":0,"map":1,"fps":58,"level":1,"end":"fine","minutes":12} - unknown ones null, the frames
// at most 999, the minutes at most 99999, the words of [A-Za-z0-9 .-]
std::string json(const Outcome& o);

}  // namespace uagfx::firstgame
