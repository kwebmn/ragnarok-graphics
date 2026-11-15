// 3.0.19-beta.51 (R-EWK4E4: moc_fild10's 3D view at 56.6 fps under its limit of 60 while the game's thread and the
// renderer each waited some 7 ms of a frame - frames lost in jerks, not to a steady load, and the 30-s averages cannot
// tell which frames or why): the long frames counted on both sides, the longest kept with its parts
#include <string>

#include "core/frame_pace.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

framepace::GameFrame gameFrame(double made, double own, double renderer) {
    framepace::GameFrame f;
    f.made = made;
    f.own = own;
    f.renderer = renderer;
    return f;
}

bool has(const std::string& line, const std::string& part) { return line.find(part) != std::string::npos; }

}  // namespace

TEST(frame_pace_game_side_counts_the_long_frames_and_keeps_the_longest) {
    framepace::GameTally t;
    CHECK(t.line().empty());
    for (int i = 0; i < 100; ++i) t.add(gameFrame(10.0, 1.7, 5.0));
    framepace::GameFrame read = gameFrame(40.0, 1.0, 5.0);  // 46: the game's own frame, two archive reads in it
    read.reads = 2;
    read.readMs = 30.0;
    read.slowest = "data\\sprite\\a.spr";
    read.slowestMs = 25.0;
    t.add(read);
    t.add(gameFrame(5.0, 1.0, 60.0));   // 66: the renderer's present
    t.add(gameFrame(20.0, 1.0, 10.0));  // 31: the game's own frame
    t.add(gameFrame(12.0, 1.0, 11.0));  // 24: not long
    const std::string line = t.line();
    CHECK(t.frames() == 104 && t.longOnes() == 3);
    CHECK(has(line, "3 of 104 over 25 ms, 1 over 50"));
    CHECK(has(line, "the longest 66.0 ms - the game's own frame 5.0, our present 1.0, the renderer's 60.0"));
    CHECK(has(line, "the longest part of the long ones - the game's own frame 2, the renderer's 1, ours 0"));
    CHECK(has(line, "archive reads in 1 of them (2, 30.0 ms; the slowest data\\sprite\\a.spr 25.0 ms)"));
    t.clear();
    CHECK(t.line().empty() && t.frames() == 0);
}

TEST(frame_pace_game_side_without_long_frames_tells_the_longest) {
    framepace::GameTally t;
    for (int i = 0; i < 9; ++i) t.add(gameFrame(10.0, 1.0, 5.0));
    t.add(gameFrame(12.0, 2.0, 6.0));
    CHECK(t.line() == "frames' pace, the game's side: none of 10 over 25 ms; the longest 20.0 ms");
}

TEST(frame_pace_renderer_side_tells_waiting_from_drawing) {
    framepace::DrawnTally t;
    framepace::DrawnFrame fine;
    fine.ms = 16.7;
    fine.drawing = 8.0;
    fine.pace = 7.0;
    fine.present = 0.4;
    for (int i = 0; i < 50; ++i) t.add(fine);
    framepace::DrawnFrame late;  // the renderer waited for the game's next frame
    late.ms = 70.0;
    late.drawing = 8.0;
    late.present = 0.5;
    CHECK(late.waiting() > 61.49 && late.waiting() < 61.51);
    t.add(late);
    framepace::DrawnFrame heavy;  // its own drawing
    heavy.ms = 33.0;
    heavy.drawing = 30.0;
    heavy.present = 0.5;
    heavy.part = "the own ground";
    heavy.partMs = 20.0;
    t.add(heavy);
    const std::string line = t.line();
    CHECK(t.frames() == 52 && t.longOnes() == 2);
    CHECK(has(line, "frames' pace, the renderer's side: 2 of 52 over 25 ms, 1 over 50"));
    CHECK(has(line, "the longest 70.0 ms - waiting for the game's frame 61.5, its drawing 8.0, the frame limit's "
                    "wait 0.0, the present 0.5"));
    CHECK(has(line, "the longest part of the long ones - waiting for the game's frame 1, its drawing 1, the frame "
                    "limit's wait 0, the present 0"));
    CHECK(has(line, "the dearest of the drawing in them - the own ground 1 (20.0 ms at most)"));
}
