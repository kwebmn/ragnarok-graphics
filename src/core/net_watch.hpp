#pragma once
#include <cstdint>
#include <string>

namespace uagfx::netwatch {

// 3.0.11-beta.122 (the owner: «иногда бывают сетевые лаги ... ощущается какбуто персонаж и мобы телепортируются»;
// «начинай по очереди» - the network ideas 1, 2 and 7): the connection to the map server watched as Windows' TCP keeps
// it (SIO_TCP_INFO: the smoothed round trip, the bytes sent again, the time-outs - each one TCP waiting for a lost
// packet, the world jumping after it), the pauses between the server's packets and its position fixes (0x0088) - for
// the log, the reports and the interface's badge. No Windows here (win/net_watch).

struct Tcp {  // a reading of the socket (TCP_INFO_v0's)
    uint32_t rttUs = 0, minRttUs = 0;
    uint64_t bytesRetrans = 0;
    uint32_t timeouts = 0;
};

// The badge's: not known yet; under 120 ms; under 250 ms; 250 ms and more or bytes sent again in the last 10 s; a
// time-out in the last 5 s
enum class Level : uint8_t { Unknown, Good, Fair, Bad, Stall };

class Watch {
public:
    void tcp(const Tcp& t, uint64_t nowMs);  // a reading (about each second)
    void packet(uint64_t nowMs);             // a packet from the server
    void fix(bool own, int cells, uint64_t nowMs);  // a position fix - the character's own or another's, how far
    Level level(uint64_t nowMs) const;
    int rttMs() const { return int(last_.rttUs / 1000); }
    bool stalled();  // a new time-out since last asked (told once)
    bool due(uint64_t nowMs) const;  // a minute since the period began: its line
    std::string line(uint64_t nowMs);  // the period's words, a new period begun

private:
    bool known_ = false, stallTold_ = true;
    Tcp last_;
    uint64_t periodAt_ = 0, resentAt_ = 0, timeoutAt_ = 0, packetAt_ = 0;
    uint64_t rttSum_ = 0, resent_ = 0, longestGap_ = 0;
    uint32_t rttCount_ = 0, rttMost_ = 0, timeouts_ = 0;
    int fixes_ = 0, ownFixes_ = 0, fixMost_ = 0;
};

}  // namespace uagfx::netwatch
