// 3.0.11-beta.122 (the owner: «иногда бывают сетевые лаги ... персонаж и мобы телепортируются»; «начинай по очереди»
// - the network ideas 1, 2, 7): the connection to the map server watched (core/net_watch) - its level for the badge,
// the stalls, the log's line
#include <string>

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
