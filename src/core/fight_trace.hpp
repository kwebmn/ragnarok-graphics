#pragma once
#include <cstdint>
#include <deque>
#include <string>

namespace uagfx::fighttrace {

// 3.0.11-beta.247 (R-AMNV99: «воевал с мобом а он какбуто телепортировался ко мне. замерал и телепортировался»): what
// the server told about a unit the character fights, each with where the game showed the unit then - written to the
// log when that unit jumps (the server's fix 0x0088, or a move starting away from where it was shown), so the cause is
// read off the packets: a stop the game did not show, a hit's swing, a late move.
enum class Kind : uint8_t { Move, Fix, HitIt, HitsMe, Died };
struct Event {
    uint64_t ms = 0;
    Kind kind = Kind::Move;
    uint32_t unit = 0;
    int a = -1, b = -1, c = -1, d = -1;  // a move's from x, y, to x, y; a fix's x, y; a hit's swing, flinch, damage
    int shownX = -1, shownY = -1;        // where the game showed the unit (-1 not known)
};

// 0x0086's way <from x>.10b <from y>.10b <to x>.10b <to y>.10b <sx>.4b <sy>.4b in 6 bytes
void way(const uint8_t* p, int& fromX, int& fromY, int& toX, int& toY);

constexpr uint64_t kKeepMs = 3000;     // the history kept
constexpr uint64_t kFoughtMs = 10000;  // a unit hit (or hitting) this recently is one fought
constexpr int kDumpsMost = 20;         // a session's dumps

class Trace {
public:
    void add(const Event& e);  // hits mark the unit fought
    bool fought(uint32_t unit, uint64_t ms) const;
    // the unit's history as log lines (the newest last, times from the first), "" when it has none or was told of
    // within the last second, or the session's dumps are spent
    std::string dump(uint32_t unit, uint64_t ms, const char* why);

private:
    std::deque<Event> events_;
    struct Seen {
        uint32_t unit = 0;
        uint64_t hitMs = 0, toldMs = 0;
    };
    std::deque<Seen> units_;
    int dumps_ = 0;
    Seen* find(uint32_t unit);
};

std::string line(const Event& e, uint64_t first);

}  // namespace uagfx::fighttrace
