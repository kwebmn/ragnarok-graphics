// 3.0.11-beta.122 (the owner: «иногда бывают сетевые лаги ... персонаж и мобы телепортируются»; «начинай по очереди»
// - the network ideas 1, 2, 7): the connection to the map server watched (core/net_watch) - its level for the badge,
// the stalls, the log's line
#include <string>
#include <vector>

#include "core/net_watch.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

netwatch::Tcp tcp(uint32_t rttMs, uint64_t resent, uint32_t timeouts) {
    netwatch::Tcp t;
    t.rttUs = rttMs * 1000;
    t.minRttUs = 30000;
    t.bytesRetrans = resent;
    t.timeouts = timeouts;
    return t;
}

bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

}  // namespace

TEST(net_watch_levels_by_the_round_trip) {
    netwatch::Watch w;
    CHECK(w.level(1000) == netwatch::Level::Unknown);
    w.tcp(tcp(45, 0, 0), 1000);
    CHECK(w.level(1000) == netwatch::Level::Good && w.rttMs() == 45);
    w.tcp(tcp(160, 0, 0), 2000);
    CHECK(w.level(2000) == netwatch::Level::Fair);
    w.tcp(tcp(320, 0, 0), 3000);
    CHECK(w.level(3000) == netwatch::Level::Bad);
}

TEST(net_watch_stalls_and_resent_bytes) {
    netwatch::Watch w;
    w.tcp(tcp(45, 0, 0), 1000);
    w.tcp(tcp(45, 1200, 0), 2000);  // bytes sent again: lost on the way
    CHECK(w.level(2000) == netwatch::Level::Bad);
    CHECK(w.level(13000) == netwatch::Level::Good);  // 10 s later: past
    w.tcp(tcp(45, 1200, 1), 14000);                  // a time-out: TCP waited for a lost packet
    CHECK(w.level(14000) == netwatch::Level::Stall && w.stalled());
    CHECK(!w.stalled());  // told once
    CHECK(w.level(18000) == netwatch::Level::Stall && w.level(20000) != netwatch::Level::Stall);  // 5 s
}

TEST(net_watch_the_logs_line) {
    netwatch::Watch w;
    CHECK(!w.due(1000));
    w.tcp(tcp(40, 0, 0), 1000);
    w.packet(1000);
    w.packet(4500);  // a pause of 3.5 s between the server's packets
    w.tcp(tcp(80, 300, 1), 30000);
    w.fix(true, 2, 31000);
    w.fix(false, 5, 32000);
    w.fix(false, 1, 33000);
    CHECK(!w.due(60000) && w.due(61000));  // a minute after the first reading
    const std::string line = w.line(61000);
    CHECK(has(line, "round trip 60 ms") && has(line, "at most 80"));  // 40 and 80 on average
    CHECK(has(line, "300 bytes sent again") && has(line, "1 time-out"));
    CHECK(has(line, "the longest pause between its packets 3.5 s"));
    CHECK(has(line, "3 position fixes") && has(line, "yours 1") && has(line, "5 cells at most"));
    CHECK(!w.due(62000));  // a new minute
    const std::string quiet = w.line(122000);
    CHECK(has(quiet, "0 position fixes") && has(quiet, "0 time-outs"));
}

// 3.0.11-beta.127 (the owner: «пинги вынеси в отдельную плашку, можешь еще график строить. но небольшую»): the last
// minute's round trips for the plate's graph - a reading each (about a second), a time-out's reading -1 (a stall); 60
// at most, the oldest first
TEST(net_watch_keeps_a_minute_for_the_graph) {
    netwatch::Watch w;
    CHECK(w.graph().empty());
    netwatch::Tcp t;
    t.rttUs = 48000;
    w.tcp(t, 1000);
    t.rttUs = 52000;
    w.tcp(t, 2000);
    t.timeouts = 1;  // TCP waited for a lost packet in this one
    w.tcp(t, 3000);
    std::vector<int> g = w.graph();
    CHECK(g.size() == 3);
    if (g.size() == 3) CHECK(g[0] == 48 && g[1] == 52 && g[2] == -1);
    t.rttUs = 40000;
    for (int i = 0; i < 70; ++i) w.tcp(t, 4000 + uint64_t(i) * 1000);
    g = w.graph();
    CHECK(g.size() == size_t(netwatch::kGraph) && g.front() == 40 && g.back() == 40);
}

// 3.0.11-beta.128 (the network plan's step 5; the owner: «делать сеть, оно бывает глючит»): a server's packet lost on
// the way holds the ones after it until it is sent again - they come late at once. A server-ticked packet 250 ms or
// more behind the game's server clock is held (the late ones within a second one hold): told once, «обрив» 5 s, the
// reading after it red on the graph, counted in the minute's line
TEST(net_watch_the_servers_held_packets) {
    netwatch::Watch w;
    w.tcp(tcp(45, 0, 0), 1000);
    w.late(40, 1500);  // on time (the way's own)
    CHECK(!w.held() && w.level(1500) == netwatch::Level::Good);
    w.late(380, 2000);  // held by a lost packet
    w.late(370, 2010);  // the same hold's others
    CHECK(w.held() && !w.held());  // told once
    CHECK(w.holdMs() == 380);
    CHECK(w.level(2100) == netwatch::Level::Stall && w.level(7100) != netwatch::Level::Stall);
    w.tcp(tcp(45, 0, 0), 2500);  // the reading after it
    const std::vector<int> g = w.graph();
    CHECK(g.size() == 2 && g.back() == -1);
    w.tcp(tcp(45, 0, 0), 3500);
    CHECK(w.graph().back() == 45);
    w.late(300, 30000);  // another, later
    CHECK(w.held());
    const std::string line = w.line(61000);
    CHECK(has(line, "2 holds by the server's lost packets (at most 380 ms)"));
    CHECK(has(w.line(122000), "0 holds"));
}

// 3.0.11-beta.131 (R-6MF4UU: «held 374 ms» right as the report's screenshot stood the game still - not the network):
// a packet's lateness less the time between the game's two last reads of its socket - what the game itself kept
// waiting is not the server's hold
TEST(net_watch_the_games_own_pause_is_no_hold) {
    CHECK(netwatch::onTheWay(374, 950) < 250);  // the game stood still almost a second: its own
    CHECK(netwatch::onTheWay(269, 14) >= 250);  // a frame between reads: the server's packets held
    CHECK(netwatch::onTheWay(40, 14) == 26);
}

// 3.0.11-beta.134 (R-U436CU: «held 352 ms» as the game came back after 5 minutes behind other windows at 0.4 fps - the
// packets waiting in the game read over several frames): after a pause of 300 ms or more between the game's reads
// its next 2 s tell no hold
TEST(net_watch_a_backlog_tells_no_hold) {
    CHECK(netwatch::inBacklog(5000, 4000));   // a second after the long pause
    CHECK(!netwatch::inBacklog(6500, 4000));  // past 2 s
    CHECK(!netwatch::inBacklog(5000, 0));     // none
}
