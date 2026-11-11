#include <string>

#include "core/part_clock.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool has(const std::string& line, const std::string& part) { return line.find(part) != std::string::npos; }

}  // namespace

// 3.0.18-beta.169 (R-45Z47V: the driver's own work at a present grew from 2.1 to 4.7 ms in five minutes of hunting,
// the reads cheap all along - which part?): each part of a present timed from the mark before it to its own, summed by
// name, a frame's average each, the dearest first
TEST(part_clock_sums_each_part_a_frame_the_dearest_first) {
    partclock::Clock c;
    for (int f = 0; f < 4; ++f) {
        c.begin(100.0 * f);
        c.mark("hotbar", 100.0 * f + 0.25);  // 0.25 ms since the begin
        c.mark("chat", 100.0 * f + 1.25);    // 1.00 ms
        c.mark("party", 100.0 * f + 1.30);   // 0.05 ms
        c.frame();
    }
    const std::string line = c.take(3);
    CHECK(has(line, "the driver's parts: 1.30 ms a frame in 3"));
    CHECK(has(line, "chat 1.00"));
    CHECK(has(line, "hotbar 0.25"));
    CHECK(has(line, "party 0.05"));
    CHECK(line.find("chat") < line.find("hotbar") && line.find("hotbar") < line.find("party"));
    CHECK(c.take(3).empty());  // started again: no frame since
}

TEST(part_clock_sums_a_name_marked_twice_and_tells_the_top_alone) {
    partclock::Clock c;
    c.begin(0);
    c.mark("chat", 1.0);
    c.mark("hotbar", 1.5);
    c.mark("chat", 3.5);  // the same part again later in the frame: summed
    c.mark("a", 3.6);
    c.mark("b", 3.7);
    c.frame();
    const std::string line = c.take(2);
    CHECK(has(line, "chat 3.00"));
    CHECK(has(line, "hotbar 0.50"));
    CHECK(!has(line, " a 0.10") && !has(line, " b 0.10"));  // past the top two
    CHECK(has(line, "the rest 0.20"));
}

TEST(part_clock_ignores_marks_outside_a_begun_frame) {
    partclock::Clock c;
    c.mark("chat", 5.0);  // no begin: nothing to measure from
    c.frame();
    CHECK(has(c.take(4), "0.00 ms a frame"));
}
