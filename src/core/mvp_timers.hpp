#pragma once
#include <string>
#include <vector>

namespace uagfx::mvptimers {

// 3.0.11-beta.280 (the owner's feature 9): the bosses the character saw die and when each comes back - the server's
// rule (reference-uathena-respawn: a boss respawns exactly its spawn's first delay after its death, no window). Kept
// in uaro-graphics\mvp.txt across the game's starts: one line a boss «<mob> <map> <died unix> <respawn ms>».
struct Timer {
    int mob = 0;
    std::string map;
    long long died = 0;  // unix seconds
    int respawnMs = 0;
    long long back() const { return died + respawnMs / 1000; }
};

class Timers {
public:
    // A boss died: one per boss and map (the newer death wins); false: nothing new
    bool died(int mob, const std::string& map, long long now, int respawnMs);
    void remove(int mob, const std::string& map);
    // The ones to show, the soonest first; dropped: the ones back more than 30 minutes ago
    std::vector<Timer> shown(long long now) const;
    void prune(long long now);
    std::string text() const;
    static Timers parse(const std::string& text);
    bool empty() const { return list_.empty(); }

private:
    std::vector<Timer> list_;
};
constexpr long long kKeepAfter = 30 * 60;

// «1:05:20», «12:34»; past: «+3:10» (back that long ago, maybe killed again)
std::string left(long long back, long long now);

}  // namespace uagfx::mvptimers
