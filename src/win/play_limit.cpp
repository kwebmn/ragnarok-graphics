#include "win/play_limit.hpp"

#include <windows.h>

#include <cstring>
#include <string>

#include "win/log.hpp"

namespace uagfx::win::playlimitwin {
namespace {

constexpr uint16_t kOwnLine = 0x008e;  // <len>.W <words>: the server's words to the character (uAthena
                                       // clif_displaymessage)

struct State {
    SRWLOCK lock = SRWLOCK_INIT;
    bool said = false;
    uint32_t saidMs = 0;
    playlimit::Notice notice;
    bool kicked = false;
    uint32_t kickedMs = 0;
};

State& state() {
    static State s;
    return s;
}

struct Locked {
    explicit Locked(State& s) : s_(s) { AcquireSRWLockExclusive(&s_.lock); }
    ~Locked() { ReleaseSRWLockExclusive(&s_.lock); }
    State& s_;
};

}  // namespace

void packet(uint16_t id, const uint8_t* p) {
    if (id != kOwnLine || !p) return;
    uint16_t len = 0;
    std::memcpy(&len, p + 2, 2);
    if (len <= 4 || len > 512) return;
    const char* text = reinterpret_cast<const char*>(p + 4);
    const std::string words(text, strnlen(text, size_t(len - 4)));
    playlimit::Notice n;
    if (!playlimit::read(words, n)) return;
    State& s = state();
    {
        Locked l(s);
        if (n.reached) {
            s.said = true;
            s.saidMs = GetTickCount();
            s.notice = n;
        }
    }
    if (n.reached)
        LOG_INFO("play limit: the server's daily limit (%d h) reached - it disconnects in %d s, resets in %d min",
                 n.hours, n.kickInS, n.resetMin);
    else
        LOG_INFO("play limit: %d min of the day's play time left (the server's words)", n.minutesLeft);
}

bool kick(const nettrail::Drop& d, playlimit::Notice& n, int& pastMin) {
    State& s = state();
    Locked l(s);
    if (!s.said || !playlimit::kickOf(d, s.saidMs, s.notice)) return false;
    n = s.notice;
    pastMin = int((GetTickCount() - s.saidMs) / 60000);
    s.kicked = true;
    s.kickedMs = GetTickCount();
    return true;
}

bool kickedWithin(uint32_t ms) {
    State& s = state();
    Locked l(s);
    return s.kicked && GetTickCount() - s.kickedMs <= ms;
}

}  // namespace uagfx::win::playlimitwin
