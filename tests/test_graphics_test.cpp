#include <cmath>
#include <map>
#include <string>

#include "core/graphics_test.hpp"
#include "test.hpp"

using namespace uagfx;
using graphics_test::Run;
using graphics_test::Step;

namespace {

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

bool near(double a, double b, double by = 0.01) { return std::fabs(a - b) <= by; }

struct Seen {
    int setBegins = 0, measureBegins = 0, measureEnds = 0, finished = 0;
    int firstSet[graphics_test::kSets];
    Seen() {
        for (int& s : firstSet) s = -1;
    }
};

// the game presenting at `fps` from `from` to `to` seconds; each step's events counted
Seen play(Run& run, double from, double to, double fps) {
    Seen s;
    const double ms = 1000.0 / fps;
    for (double t = from; t < to; t += ms / 1000.0) {
        const Step step = run.frame(t, ms);
        if (step.setBegins) {
            ++s.setBegins;
            s.firstSet[step.phase] = int(t);
        }
        s.measureBegins += step.measureBegins ? 1 : 0;
        s.measureEnds += step.measureEnds ? 1 : 0;
        s.finished += step.finished ? 1 : 0;
    }
    return s;
}

}  // namespace

// 3.0.5-beta.6 (the user: «есть также идея сделать некий режим теста в игре ... чтобы не стоять кучу времени»):
// the sets, each 3 s settling and 12 s measured, then the end - once. 3.0.5-beta.14: seven (High twice more, without
// the hills, without the materials)
TEST(graphics_test_runs_each_set_settling_then_measured_and_ends_once) {
    Run run;
    run.start(100.0);
    CHECK(run.running());
    const Seen s = play(run, 100.0, 220.0, 60.0);
    CHECK_EQ(graphics_test::kSets, 7);
    CHECK_EQ(s.setBegins, 7);
    CHECK_EQ(s.measureBegins, 7);
    CHECK_EQ(s.measureEnds, 7);
    CHECK_EQ(s.finished, 1);
    CHECK(!run.running());
    for (int p = 0; p < graphics_test::kSets; ++p) CHECK_EQ(s.firstSet[p], 100 + 15 * p);
    // a set measured for its 12 s at 60 frames a second: 720 frames or so
    for (int p = 0; p < graphics_test::kSets; ++p) {
        CHECK(run.result(p).frames >= 718 && run.result(p).frames <= 722);
        CHECK(near(run.result(p).fps, 60.0, 0.1));
        CHECK(near(run.result(p).seconds, 12.0, 0.05));
    }
}

TEST(graphics_test_measures_the_slowest_tenth_of_the_frames_as_frames_a_second) {
    Run run;
    run.start(0.0);
    for (double t = 0.0; t < 2.9; t += 0.01) run.frame(t, 10.0);  // settling
    // measured: 90 frames of 10 ms, 10 of 40 ms - 1.3 s; the slowest tenth 25 frames a second
    double t = 3.5;
    for (int k = 0; k < 100; ++k) {
        const double ms = k % 10 == 9 ? 40.0 : 10.0;
        t += ms / 1000.0;
        run.frame(t, ms);
    }
    const graphics_test::PhaseResult& r = run.result(0);
    CHECK_EQ(r.frames, 100);
    CHECK(near(r.seconds, 1.3));
    CHECK(near(r.fps, 100.0 / 1.3));
    CHECK(near(r.slowFps, 25.0));
}

// the card's half-second averages: none of the settling's, none of the first second measured (it may hold them)
TEST(graphics_test_averages_the_cards_work_from_its_measured_window_alone) {
    Run run;
    run.start(0.0);
    std::map<std::string, double> fx{{"grass", 4.0}};
    double t = 0.0;
    for (; t < 3.5; t += 0.01) run.frame(t, 10.0);
    run.gpu(1.0, 50.0, fx);  // settling
    run.gpu(3.5, 40.0, fx);  // the first second measured
    for (; t < 6.0; t += 0.01) run.frame(t, 10.0);
    run.gpu(4.5, 8.0, {{"grass", 2.0}, {"glow", 1.0}});
    run.gpu(5.0, 10.0, {{"grass", 4.0}});
    const graphics_test::PhaseResult& r = run.result(0);
    CHECK(near(r.gpuMs, 9.0));
    CHECK(near(r.effects.at("grass"), 3.0));
    CHECK(near(r.effects.at("glow"), 0.5));  // timed in one of the two: 0 in the other
    CHECK(near(run.result(1).gpuMs, -1.0));  // not measured
}

TEST(graphics_test_stops_without_an_end) {
    Run run;
    run.start(0.0);
    run.frame(1.0, 16.0);
    run.stop();
    CHECK(!run.running());
    const Step after = run.frame(80.0, 16.0);
    CHECK(!after.finished && !after.setBegins && !after.measuring);
}

// the camera: a full turn from the player's own over the 12 s, out to the far end and back as a half sine
TEST(graphics_test_camera_turns_once_and_goes_out_and_back_in) {
    graphics_test::Pose p = graphics_test::poseAt(300.0f, 300.0f, 1400.0f, 0.0);
    CHECK(near(p.yaw, 300.0) && near(p.distance, 300.0));
    p = graphics_test::poseAt(300.0f, 300.0f, 1400.0f, 0.5);
    CHECK(near(p.yaw, 120.0) && near(p.distance, 1400.0));  // 300 + 180, past 360
    p = graphics_test::poseAt(300.0f, 300.0f, 1400.0f, 0.25);
    CHECK(near(p.yaw, 30.0) && near(p.distance, 300.0 + 1100.0 * std::sqrt(0.5), 0.5));
    p = graphics_test::poseAt(300.0f, 300.0f, 1400.0f, 1.0);
    CHECK(near(p.yaw, 300.0, 0.01) && near(p.distance, 300.0, 0.01));
    CHECK(p.yaw >= 0.0f && p.yaw < 360.0f);
}

TEST(graphics_test_camera_keeps_to_the_games_own_distances_of_the_map) {
    float from = 0.0f, to = 0.0f;
    graphics_test::zoomRange(true, false, 0, 0, from, to);
    CHECK(near(from, 300.0) && near(to, 1400.0));
    graphics_test::zoomRange(false, false, 0, 0, from, to);  // a building: the game's 230..300
    CHECK(near(from, 230.0) && near(to, 300.0));
    graphics_test::zoomRange(true, true, 150, 250, from, to);  // the map's own limits (CGameMode+0x288, +0x28a)
    CHECK(near(from, 150.0) && near(to, 400.0));
    graphics_test::zoomRange(true, true, -5, 0, from, to);  // nonsense: outdoors' own
    CHECK(near(from, 300.0) && near(to, 1400.0));
}

TEST(graphics_test_tags_each_sets_profile_window) {
    CHECK(graphics_test::tagOf(0) == "test 1/7 off");
    CHECK(graphics_test::tagOf(1) == "test 2/7 low");
    CHECK(graphics_test::tagOf(3) == "test 4/7 high");
    CHECK(graphics_test::tagOf(4) == "test 5/7 high-no-hills");
    CHECK(graphics_test::tagOf(5) == "test 6/7 high-no-materials");
    CHECK(graphics_test::tagOf(6) == "test 7/7 own");
}

// the renderers' lines for a set's window, read back from the logs; other lines between them left alone
TEST(graphics_test_reads_the_tagged_profile_lines_back_from_a_log) {
    const std::string log =
        "12:00:01.000 INFO  profile: Direct3D 12 - 600 frames in 10.0 s (60.0 a second); a frame 16.7 ms\n"
        "12:00:01.001 INFO  profile: the card 9.00 ms a frame - the game's world 5.00 (ms)\n"
        "12:00:15.000 INFO  profile [test 2/5 low]: Direct3D 12 - 720 frames in 12.0 s (60.0 a second); a frame\n"
        "12:00:15.001 INFO  map: prontera\n"
        "12:00:15.002 INFO  profile [test 2/5 low]: the card 6.20 ms a frame - grass 2.10, the game's world 1.90, "
        "the interface 0.60, glow 0.01 (ms)\n"
        "12:00:15.003 INFO  profile [test 2/5 low]: the processor 3.00 ms a frame in the spans - x 3.00 (ms)\n"
        "12:00:30.002 INFO  profile [test 4/5 high]: the card 22.50 ms a frame - grass 12.25, ao 4.00 (ms)\n";
    const std::map<int, graphics_test::Parts> parts = graphics_test::profileParts(log);
    CHECK_EQ(int(parts.size()), 2);
    CHECK(near(parts.at(1).cardMs, 6.2));
    CHECK_EQ(int(parts.at(1).parts.size()), 4);
    CHECK(parts.at(1).parts[0].first == "grass" && near(parts.at(1).parts[0].second, 2.1));
    CHECK(parts.at(1).parts[1].first == "the game's world");
    CHECK(near(parts.at(3).cardMs, 22.5) && parts.at(3).parts[1].first == "ao");
}

TEST(graphics_test_summary_is_a_short_table_of_the_sets) {
    graphics_test::Head h;
    h.version = "3.0.5-beta.6";
    h.map = "prt_fild08";
    h.api = "Direct3D 12";
    h.gpu = "AMD Radeon(TM) 610M";
    h.width = 1920;
    h.height = 1080;
    h.fpsLimit = 60;
    graphics_test::PhaseResult r[graphics_test::kSets];
    r[0].frames = 720;
    r[0].fps = 60.0;
    r[0].slowFps = 55.4;
    r[0].gpuMs = 3.14;
    r[1].frames = 600;
    r[1].fps = 50.0;
    r[1].slowFps = 41.0;
    r[1].gpuMs = 18.0;
    r[1].effects = {{"grass", 9.0}, {"glow", 0.5}, {"rays", 1.5}};  // no profile lines: the card's timed passes
    std::map<int, graphics_test::Parts> parts;
    parts[0].cardMs = 3.1;
    parts[0].parts = {{"the game's world", 1.9}, {"the interface", 0.6}, {"the picture into the window", 0.3},
                      {"clears", 0.1}};
    const std::string uk = graphics_test::summary(h, r, parts, settings::Lang::Uk);
    CHECK(has(uk, "Тест графіки"));
    CHECK(has(uk, "prt_fild08") && has(uk, "Direct3D 12") && has(uk, "AMD Radeon(TM) 610M") && has(uk, "1920×1080"));
    CHECK(has(uk, "60"));  // the frame limit
    CHECK(has(uk, "Без ефектів") && has(uk, "Низькі") && has(uk, "Середні") && has(uk, "Високі") &&
          has(uk, "Високі без пагорбів") && has(uk, "Високі без матеріалів") && has(uk, "Ваші налаштування"));
    // the first set: its profile's three dearest parts; the second: the card's timed passes, the dearest first
    CHECK(has(uk, "the game's world 1.90, the interface 0.60, the picture into the window 0.30"));
    CHECK(!has(uk, "clears 0.10"));
    CHECK(has(uk, "grass 9.00, rays 1.50, glow 0.50"));
    CHECK(has(uk, "3.1") && has(uk, "18.0"));
    CHECK(has(uk, "не виміряно"));  // the sets with no frames
    CHECK(uk.size() < 4000);       // the site's comment
    const std::string en = graphics_test::summary(h, r, parts, settings::Lang::En);
    CHECK(has(en, "Graphics test") && has(en, "Your settings"));
}

// a table in a fixed-width font: the columns line up by the letters, not the bytes (Cyrillic is two bytes)
TEST(graphics_test_summary_columns_line_up_by_letters) {
    graphics_test::Head h;
    graphics_test::PhaseResult r[graphics_test::kSets];
    for (auto& one : r) {
        one.frames = 100;
        one.fps = 60.0;
        one.slowFps = 50.0;
        one.gpuMs = 5.0;
    }
    const std::string uk = graphics_test::summary(h, r, {}, settings::Lang::Uk);
    // the rows of the sets: their figures start at the same letter
    std::vector<size_t> at;
    size_t from = 0;
    while (true) {
        const size_t end = uk.find('\n', from);
        const std::string row = uk.substr(from, end == std::string::npos ? std::string::npos : end - from);
        const size_t figure = row.find("60");
        if (figure != std::string::npos && row.find("50") != std::string::npos) {
            size_t letters = 0;
            for (size_t i = 0; i < figure; ++i) letters += (static_cast<unsigned char>(row[i]) & 0xC0) != 0x80;
            at.push_back(letters);
        }
        if (end == std::string::npos) break;
        from = end + 1;
    }
    CHECK_EQ(int(at.size()), 7);
    for (size_t k = 1; k < at.size(); ++k) CHECK_EQ(at[k], at[0]);
}

// 3.0.5-beta.7 (the user: «сделай» - the settings app's button): «Гра» shows the last test's table from
// graphics-test.txt, read back into its cells (the columns are two spaces apart at least)
TEST(graphics_test_summary_reads_back_into_its_cells) {
    graphics_test::Head h;
    h.version = "3.0.5-beta.7";
    h.map = "prt_fild08";
    graphics_test::PhaseResult r[graphics_test::kSets];
    const double fps[] = {60.0, 60.0, 34.0, 0.0, 23.0, 26.0, 33.0}, slow[] = {57.0, 52.0, 28.0, 0.0, 20.0, 22.0, 27.0};
    for (int p = 0; p < graphics_test::kSets; ++p) {
        r[p].frames = p == 3 ? 0 : 700;
        r[p].fps = fps[p];
        r[p].slowFps = slow[p];
        r[p].gpuMs = 6.2 + p;
    }
    std::map<int, graphics_test::Parts> parts;
    parts[0].parts = {{"the game's world", 3.9}, {"the interface", 1.4}, {"the picture into the window", 0.5}};
    const std::string text = graphics_test::summary(h, r, parts, settings::Lang::Uk);
    graphics_test::Table t;
    CHECK(graphics_test::parseSummary(text, t));
    CHECK(t.head.rfind("Тест графіки · 3.0.5-beta.7 · prt_fild08", 0) == 0);
    CHECK_EQ(int(t.columns.size()), 5);
    CHECK(t.columns[0] == "Набір" && t.columns[2] == "найповільніша десята, FPS");
    CHECK(t.columns[4] == "найдорожче на відеокарті, мс");
    CHECK_EQ(int(t.rows.size()), 7);
    CHECK(t.rows[0].size() == 5 && t.rows[0][0] == "Без ефектів" && t.rows[0][1] == "60" && t.rows[0][3] == "6.2");
    CHECK(t.rows[0][4] == "the game's world 3.90, the interface 1.40, the picture into the window 0.50");
    CHECK(t.rows[3].size() == 2 && t.rows[3][0] == "Високі" && t.rows[3][1] == "не виміряно");
    CHECK(t.rows[4][0] == "Високі без пагорбів" && t.rows[4][1] == "23");
    CHECK(t.rows[5][0] == "Високі без матеріалів" && t.rows[5][1] == "26");
    CHECK(t.rows[6][0] == "Ваші налаштування" && t.rows[6][1] == "33");
    CHECK(!graphics_test::parseSummary("", t));
    CHECK(!graphics_test::parseSummary("a line of something else\n", t));
}

// 3.0.5-beta.8: a set without profile lines (the profile not run) - the card's timed passes by the profile's words
TEST(graphics_test_summary_names_the_timed_passes_as_the_profile_does) {
    graphics_test::Head h;
    graphics_test::PhaseResult r[graphics_test::kSets];
    r[4].frames = 700;
    r[4].fps = 60.0;
    r[4].slowFps = 54.0;
    r[4].gpuMs = 10.0;
    r[4].effects = {{"steps", 3.98}, {"world", 2.84}, {"actor_shadows", 0.67}, {"grade", 0.5}};
    const std::string uk = graphics_test::summary(h, r, {}, settings::Lang::Uk);
    CHECK(has(uk, "footprints 3.98, the game's world 2.84, characters' shadows 0.67"));
}

// 3.0.5-beta.14 (the user: «делать» - what of «Високі»'s world costs the card): under the table the High set's cost of
// each of its four - the seams' strips and the world's reflections their own spans of the profile, the hills and the
// materials what the card does less without them (the two sets High without each); none not measured
TEST(graphics_test_summary_says_what_four_of_high_cost_the_card) {
    graphics_test::Head h;
    graphics_test::PhaseResult r[graphics_test::kSets];
    for (int p = 0; p < graphics_test::kSets; ++p) {
        r[p].frames = 600;
        r[p].fps = 30.0;
        r[p].slowFps = 25.0;
    }
    r[3].gpuMs = 47.8;  // High
    r[4].gpuMs = 44.0;  // High without the hills
    r[5].gpuMs = 40.1;  // High without the materials
    std::map<int, graphics_test::Parts> parts;
    parts[3].cardMs = 47.8;
    parts[3].parts = {{"the game's world", 18.0}, {"ground seams' strips", 2.31}, {"world reflections", 1.66}};
    const std::string uk = graphics_test::summary(h, r, parts, settings::Lang::Uk);
    CHECK(has(uk, "Ціна у «Високих» на відеокарті, мс: «Стики трави й доріг без сходинок» 2.3, «Пагорби без "
                  "сходинок світла» 3.8, «Матеріали: камінь, цегла, кора» 7.7, «Відображення світу у воді» 1.7"));
    graphics_test::Table t;  // the table read back as before, the line under it left alone
    CHECK(graphics_test::parseSummary(uk, t));
    CHECK_EQ(int(t.rows.size()), 7);
    const std::string en = graphics_test::summary(h, r, parts, settings::Lang::En);
    CHECK(has(en, "The card's cost within «High», ms: «Grass and road seams without steps» 2.3"));
    // High not measured: no such line; the materials' set not measured: the others alone
    graphics_test::PhaseResult none[graphics_test::kSets];
    CHECK(!has(graphics_test::summary(h, none, parts, settings::Lang::Uk), "Ціна у «Високих»"));
    r[5] = graphics_test::PhaseResult{};
    const std::string some = graphics_test::summary(h, r, parts, settings::Lang::Uk);
    CHECK(has(some, "«Пагорби без сходинок світла» 3.8, «Відображення світу у воді» 1.7"));
    CHECK(!has(some, "Матеріали: камінь, цегла, кора»"));
}
