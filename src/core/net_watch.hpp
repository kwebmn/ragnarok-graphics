#pragma once
#include <cstdint>
#include <string>
#include <vector>

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

constexpr int kGraph = 60;  // 3.0.11-beta.127: the plate's graph - the last readings (about a minute)

// 3.0.11-beta.131 (R-6MF4UU: «held 374 ms» as the report's screenshot stood the game still): a server-ticked packet's
// lateness on the way - less the time between the game's two last reads of its socket (what the game kept waiting)
inline int onTheWay(int lateMs, uint32_t readGapMs) { return lateMs - int(readGapMs); }
// 3.0.11-beta.134 (R-U436CU: «held 352 ms» as the game came back from 5 minutes behind other windows - the waiting
// packets read over several frames): after a pause of kBacklogGapMs or more between the game's reads, its next
// kBacklogMs tell no hold
constexpr uint64_t kBacklogGapMs = 300, kBacklogMs = 2000;
inline bool inBacklog(uint64_t nowMs, uint64_t longGapAtMs) {
    return longGapAtMs && nowMs >= longGapAtMs && nowMs - longGapAtMs < kBacklogMs;
}

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
    // 3.0.11-beta.127 (the owner: «пинги вынеси в отдельную плашку, можешь еще график строить»): the last kGraph
    // readings' round trips (ms), the oldest first; a reading with a time-out -1 (a stall)
    std::vector<int> graph() const;
    // 3.0.11-beta.128 (the network plan's step 5; the owner: «делать сеть, оно бывает глючит»): a server-ticked
    // packet's lateness - the game's server clock less its tick (ms); 250 or more: held by a lost packet of the
    // server's (they come late at once after it is sent again; the late ones within a second one hold)
    void late(int ms, uint64_t nowMs);
    bool held();  // a new hold since last asked (told once)
    int holdMs() const { return holdMs_; }  // the last hold's lateness at most
    // 3.0.11-beta.137 (R-QTN6TF): when a packet was last lost - a time-out, bytes sent again, a hold (0 none yet)
    uint64_t lastLossMs() const { return lossAt_; }

private:
    bool known_ = false, stallTold_ = true;
    Tcp last_;
    uint64_t periodAt_ = 0, resentAt_ = 0, timeoutAt_ = 0, packetAt_ = 0;
    uint64_t rttSum_ = 0, resent_ = 0, longestGap_ = 0;
    uint32_t rttCount_ = 0, rttMost_ = 0, timeouts_ = 0;
    int fixes_ = 0, ownFixes_ = 0, fixMost_ = 0;
    int graph_[kGraph] = {};
    int graphNext_ = 0, graphCount_ = 0;
    uint64_t holdAt_ = 0, lateAt_ = 0, readAt_ = 0;  // 3.0.11-beta.128: the hold began, its last late one, a reading
    uint64_t lossAt_ = 0;                            // 3.0.11-beta.137
    int holdMs_ = 0, holds_ = 0, holdMost_ = 0;
    bool holdTold_ = true;
};

}  // namespace uagfx::netwatch
