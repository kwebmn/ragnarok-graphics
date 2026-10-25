#include <string>
#include <vector>

#include "core/frame_profile.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool has(const std::string& line, const std::string& part) { return line.find(part) != std::string::npos; }

}  // namespace

// 3.0.5-beta.2 (the user: «давай сделаем анализ, где именно идет больше всего расчетов, что вызывают "тормоза"»):
// the spans of the frames summed by their names, a frame's average each, the dearest first
TEST(frame_profile_sums_the_spans_by_name_a_frame_each_the_dearest_first) {
    frame_profile::Window w;
    for (int k = 0; k < 4; ++k) {
        w.span("the game's world", 3.0, 2.0);
        w.span("the world made larger", 8.0, 0.1);
        w.span("the game's world", 0.5, 1.0);  // the world again after a pass: the same name, summed
        w.span("the interface", 1.0, 0.5);
        w.frame(25.0);
    }
    CHECK_EQ(w.frames(), 4);
    const std::vector<std::string> lines = w.lines("Direct3D 11", 10.0);
    CHECK(lines.size() >= 3);
    CHECK(has(lines[0], "Direct3D 11") && has(lines[0], "4 frames") && has(lines[0], "25.0 ms"));
    // the card: 12.5 ms a frame - the world made larger first (8.00), then the game's world (3.50), the interface
    CHECK(has(lines[1], "12.50 ms") && has(lines[1], "the world made larger 8.00, the game's world 3.50, "
                                                      "the interface 1.00"));
    // the processor: 3.6 ms a frame - the game's world first (3.00)
    CHECK(has(lines[2], "3.60 ms") && has(lines[2], "the game's world 3.00, the interface 0.50, "
                                                     "the world made larger 0.10"));
}

TEST(frame_profile_leaves_a_span_the_card_did_not_time_out_of_its_line) {
    frame_profile::Window w;
    w.span("the game's world", 2.0, 1.0);
    w.span("the frame limit's wait", -1.0, 6.0);  // the processor's alone
    w.frame(16.7);
    const std::vector<std::string> lines = w.lines("Direct3D 12", 10.0);
    CHECK(has(lines[1], "2.00 ms") && !has(lines[1], "the frame limit's wait"));
    CHECK(has(lines[2], "7.00 ms") && has(lines[2], "the frame limit's wait 6.00"));
}

TEST(frame_profile_tells_the_processors_parts_counted_apart) {
    frame_profile::Window w;
    w.span("the game's world", 3.0, 4.0);
    w.cpu("Direct3D 11's calls", 2.5);
    w.cpu("the draws' own work", 1.5);
    w.frame(20.0);
    w.span("the game's world", 3.0, 4.0);
    w.cpu("Direct3D 11's calls", 3.5);
    w.frame(20.0);
    const std::vector<std::string> lines = w.lines("Direct3D 11", 10.0);
    CHECK_EQ(lines.size(), size_t(4));
    CHECK(has(lines[3], "Direct3D 11's calls 3.00, the draws' own work 0.75"));
}

TEST(frame_profile_tells_the_slowest_tenth_of_the_frames) {
    frame_profile::Window w;
    for (int k = 0; k < 18; ++k) w.frame(16.0);
    w.frame(40.0);
    w.frame(44.0);
    const std::vector<std::string> lines = w.lines("Direct3D 12", 10.0);
    CHECK(has(lines[0], "20 frames in 10.0 s (2.0 a second)"));
    CHECK(has(lines[0], "the slowest tenth 42.0 ms"));
}

TEST(frame_profile_starts_again_when_cleared) {
    frame_profile::Window w;
    w.span("the game's world", 3.0, 2.0);
    w.frame(16.0);
    w.clear();
    CHECK_EQ(w.frames(), 0);
    CHECK(w.lines("Direct3D 11", 10.0).empty());
}

// 3.0.5-beta.6: the graphics test's windows - each line tagged with its set, so that they are read back from the log
TEST(frame_profile_tags_every_line_of_a_window) {
    frame_profile::Window w;
    w.span("grass", 2.0, 0.5);
    w.cpu("the frame limit's wait", 3.0);
    w.frame(16.0);
    const std::vector<std::string> lines = w.lines("Direct3D 12", 12.0, "test 2/5 low");
    CHECK_EQ(int(lines.size()), 4);
    for (const std::string& line : lines) CHECK(line.rfind("profile [test 2/5 low]: ", 0) == 0);
    CHECK(has(lines[1], "]: the card 2.00 ms a frame - grass 2.00 (ms)"));
    CHECK(w.lines("Direct3D 12", 12.0)[0].rfind("profile: Direct3D 12 - ", 0) == 0);  // untagged as before
}

// every 10 s as before; a cut asked ends the window at once - logged under its tag, or dropped ("") - and while the
// cuts go on, no window of 10 s
TEST(frame_profile_flush_ends_windows_by_time_or_by_the_tests_cuts) {
    frame_profile::Flush f;
    bool log = false;
    std::string tag = "x";
    CHECK(!f.due(9999.0, log, tag));
    CHECK(f.due(10000.0, log, tag) && log && tag.empty());
    f.cut("", true);  // the test's set measured from now: the window so far dropped
    CHECK(f.due(5.0, log, tag) && !log);
    CHECK(!f.due(20000.0, log, tag));  // by cuts alone now
    f.cut("test 1/5 off", true);
    CHECK(f.due(12000.0, log, tag) && log && tag == "test 1/5 off");
    f.cut("", false);  // the test over: every 10 s again
    CHECK(f.due(1.0, log, tag) && !log);
    CHECK(f.due(10000.0, log, tag) && log && tag.empty());
    f.cut("test 2/5 low", true);
    f.reset();  // the profile turned off: nothing asked any more
    CHECK(!f.due(1.0, log, tag));
}
