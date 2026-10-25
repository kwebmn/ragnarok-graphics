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
