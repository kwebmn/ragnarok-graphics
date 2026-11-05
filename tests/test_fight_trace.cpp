#include "core/fight_trace.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.11-beta.247 (R-AMNV99): a fought unit's packets kept for the log when it jumps
TEST(fight_trace_way_and_dump) {
    // from (150, 179) to (153, 181): 150 = 0b0010010110, 179 = 0b0010110011, 153 = 0b0010011001, 181 = 0b0010110101
    uint8_t p[6] = {};
    const int fx = 150, fy = 179, tx = 153, ty = 181;
    p[0] = uint8_t(fx >> 2);
    p[1] = uint8_t(((fx & 3) << 6) | (fy >> 4));
    p[2] = uint8_t(((fy & 15) << 4) | (tx >> 6));
    p[3] = uint8_t(((tx & 63) << 2) | (ty >> 8));
    p[4] = uint8_t(ty & 255);
    int a, b, c, d;
    fighttrace::way(p, a, b, c, d);
    CHECK(a == 150 && b == 179 && c == 153 && d == 181);

    fighttrace::Trace t;
    using K = fighttrace::Kind;
    t.add({1000, K::Move, 7, 150, 179, 153, 181, 150, 179});
    CHECK(!t.fought(7, 1000) && t.dump(7, 1000, "x").empty());  // not fought: no dump
    t.add({1200, K::HitIt, 7, 600, 480, 40, -1, 151, 180});
    CHECK(t.fought(7, 1200) && !t.fought(8, 1200) && !t.fought(7, 1200 + fighttrace::kFoughtMs + 1));
    t.add({1500, K::Fix, 7, 153, 181, -1, -1, 151, 180});
    const std::string out = t.dump(7, 1500, "jumped 2 cells");
    CHECK(out.find("fight trace: 7 jumped 2 cells") == 0);
    CHECK(out.find("+0 ms walks 150, 179 -> 153, 181 (shown at 150, 179)") != std::string::npos);
    CHECK(out.find("+200 ms hit by the character (swing 600 ms, flinch 480 ms, 40)") != std::string::npos);
    CHECK(out.find("+500 ms put on 153, 181 (shown at 151, 180)") != std::string::npos);
    CHECK(t.dump(7, 1600, "again").empty());  // told within a second
    CHECK(!t.dump(7, 2600, "later").empty());
}
