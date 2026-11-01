#pragma once
#include <cstdint>
#include <deque>

namespace uagfx::exprate {

// 3.0.11-beta.141 (the owner: «блок с персонажем должен работать от альт-V, там должен быть прогресбар с бейз опытом и
// джоб опытом ... точное количество опыта. и побольше подробностей»): the character's experience gained as it plays -
// each change of the level's exp read from the game made a gain (a kill's share, a quest's), across a new level too -
// for its pace an hour and the time to the next level (core/char_block shows them). One for the base, one for the job.
constexpr uint64_t kWindowMs = 10 * 60 * 1000;  // the pace over the last ten minutes
constexpr uint64_t kLeastMs = 60 * 1000;        // measured for a minute at least

class Track {
public:
    // The exp read at `nowMs`: its level, the level's exp and the next level's (0 at the last level). More in the same
    // level - the difference gained; a level more - the rest of the old level's and the new one's; less (a death's
    // loss) or a level less (a new job) - none, counted on from there; a 0 after more in the same level (the game's
    // numbers not come yet) and level 0 (not known) not taken
    void feed(uint64_t nowMs, int level, long long exp, long long next);
    void reset();
    long long session() const { return session_; }  // gained since the first reading
    long long last() const { return last_; }        // the last gain; 0 none yet
    // Gained an hour: the last kWindowMs's gains over the time measured (kWindowMs, or since the first reading when
    // that was later); < 0 not known yet (under kLeastMs measured)
    double perHour(uint64_t nowMs) const;

private:
    struct Gain {
        uint64_t ms;
        long long exp;
    };
    bool known_ = false;
    uint64_t first_ = 0;
    int level_ = 0;
    long long exp_ = 0, next_ = 0, session_ = 0, last_ = 0;
    std::deque<Gain> gains_;
};

// The minutes to the next level from `exp` of `next` at `perHour`, rounded up; < 0 not known (no pace, the last level)
long long minutesTo(long long exp, long long next, double perHour);

}  // namespace uagfx::exprate
