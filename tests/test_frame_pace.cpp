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

// 3.0.19-beta.52 (R-GHP9SJ: on .51 the renderer's long frames had every part of their drawing slower at once - the
// thread kept from the processor, or a view with many more draws?): how much of a span the thread ran, the draws, the
// card's slot
TEST(frame_pace_ran_is_the_threads_share_of_the_span) {
    CHECK(framepace::ranOf(10.0, 5, 10) > 4.99 && framepace::ranOf(10.0, 5, 10) < 5.01);
    CHECK(framepace::ranOf(10.0, 20, 10) > 9.99 && framepace::ranOf(10.0, 20, 10) < 10.01);  // never past the span
    CHECK(framepace::ranOf(10.0, 5, 0) < 0.0);                                                // unknown
}

TEST(frame_pace_game_side_tells_how_much_its_thread_ran_and_the_draws) {
    framepace::GameTally t;
    for (int i = 0; i < 10; ++i) {
        framepace::GameFrame f = gameFrame(10.0, 1.0, 5.0);
        f.draws = 2000;
        f.ran = 9.8;
        t.add(f);
    }
    framepace::GameFrame kept = gameFrame(40.0, 1.0, 5.0);  // kept from the processor
    kept.draws = 2000;
    kept.ran = 10.0;
    t.add(kept);
    framepace::GameFrame busy = gameFrame(30.0, 1.0, 5.0);  // busy with a view of many draws
    busy.draws = 6000;
    busy.ran = 29.0;
    t.add(busy);
    framepace::GameFrame unknown = gameFrame(15.0, 1.0, 5.0);  // not long, its share unknown: left out of the shares
    unknown.draws = 2000;
    t.add(unknown);
    const std::string line = t.line();
    CHECK(has(line, "the longest 46.0 ms - the game's own frame 40.0 (ran 10.0), our present 1.0, the renderer's "
                    "5.0; 2000 draws"));
    CHECK(has(line, "draws a frame 2308 on average, 4000 in the long ones"));
    CHECK(has(line, "the game's thread ran 56 % of their own frames' time (98 % in the others)"));
}

TEST(frame_pace_renderer_side_tells_how_much_its_drawing_ran_and_the_card) {
    framepace::DrawnTally t;
    for (int i = 0; i < 10; ++i) {
        framepace::DrawnFrame f;
        f.ms = 16.7;
        f.drawing = 8.0;
        f.card = 0.1;
        f.ran = 7.8;
        f.pace = 7.0;
        f.present = 0.4;
        f.draws = 2000;
        t.add(f);
    }
    framepace::DrawnFrame kept;
    kept.ms = 47.4;
    kept.drawing = 37.7;
    kept.card = 0.1;
    kept.ran = 9.1;
    kept.pace = 3.4;
    kept.present = 0.7;
    kept.draws = 2100;
    kept.part = "the game's draws themselves";
    kept.partMs = 12.6;
    t.add(kept);
    const std::string line = t.line();
    CHECK(has(line, "its drawing 37.7 (ran 9.1, the card's slot 0.1)"));
    CHECK(has(line, "(its drawing's dearest: the game's draws themselves 12.6); 2100 draws"));
    CHECK(has(line, "draws a frame 2009 on average, 2100 in the long ones"));
    CHECK(has(line, "its drawing ran 24 % of its time in them (99 % in the others), the card's slot 0.1 ms a frame "
                    "in them"));
}

// 3.0.22-beta.2 (the owner: «когда нажимаю на нпс, оно какбуто подлагивает»): the last frames kept with their times -
// the longest round an event (an NPC's talk opening) told with its parts
TEST(frame_pace_the_longest_frame_round_an_event) {
    framepace::Recent r;
    framepace::GameFrame f;
    for (int i = 0; i < 100; ++i) {
        f = framepace::GameFrame{};
        f.made = 10.0;
        f.own = 2.0;
        if (i == 60) {
            f.made = 90.0;  // the talk's frame
            f.reads = 3;
            f.readMs = 40.0;
            f.slowest = "data\\texture\\ui\\btn_ok.bmp";
            f.slowestMs = 25.0;
        }
        r.add(f, 1000.0 + i * 16.0);
    }
    framepace::GameFrame longest;
    uint32_t frames = 0, reads = 0;
    double readMs = 0.0;
    CHECK(r.longestBetween(1000.0 + 50 * 16.0, 1000.0 + 70 * 16.0, longest, frames, reads, readMs));
    CHECK_EQ(frames, 21u);
    CHECK(longest.made == 90.0 && longest.slowest == "data\\texture\\ui\\btn_ok.bmp");
    CHECK(reads == 3u && readMs == 40.0);
    CHECK(!r.longestBetween(0.0, 500.0, longest, frames, reads, readMs));  // none then
    for (int i = 0; i < int(framepace::kRecent) + 10; ++i) r.add(f, 5000.0 + i * 16.0);
    CHECK(!r.longestBetween(1000.0 + 50 * 16.0, 1000.0 + 70 * 16.0, longest, frames, reads, readMs));  // gone
}

TEST(frame_pace_an_events_words) {
    framepace::GameFrame f;
    f.made = 70.0;
    f.ran = 52.0;
    f.own = 9.0;
    f.renderer = 6.0;
    f.slowest = "data\\texture\\ui\\btn_ok.bmp";
    f.slowestMs = 25.0;
    CHECK_EQ(framepace::eventWords(f, 180, 4, 41.0, 3.0),
             std::string("in the 3.0 s round it 180 frames, the longest 85.0 ms - the game's own 70.0 (ran 52.0), "
                         "ours 9.0, the renderer's 6.0; archive reads 4 (41.0 ms), the slowest "
                         "data\\texture\\ui\\btn_ok.bmp 25.0 ms"));
    f.ran = -1.0;
    CHECK_EQ(framepace::eventWords(f, 10, 0, 0.0, 3.0),
             std::string("in the 3.0 s round it 10 frames, the longest 85.0 ms - the game's own 70.0, ours 9.0, "
                         "the renderer's 6.0; no archive reads"));
}
