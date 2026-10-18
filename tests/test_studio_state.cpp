#include <string>
#include <utility>
#include <vector>

#include "core/studio_state.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(session_round_trips_and_defaults) {
    // 2.3 (G5): a new studio opens where the last one stood
    studio::Session s;
    s.tab = "pack";
    s.nav = "map:cmd_fild02";
    s.filter = "sand";
    s.sort = "detail";
    s.sortDown = true;
    s.selected = {"256x256\\0123456789abcdef", "64x64\\00000000000000aa"};
    s.shown = "256x256\\0123456789abcdef";
    s.zoom = 8;
    s.view = "wipe";
    s.x = 10, s.y = 20, s.w = 1600, s.h = 900;
    s.paused = true;
    s.chain = "anime x4 tone grain=0.30";
    json::Value v;
    std::string error;
    CHECK(json::parse(json::dump(studio::toJson(s)), v, error));
    const studio::Session b = studio::sessionOf(v);
    CHECK(b.tab == "pack" && b.nav == s.nav && b.filter == "sand" && b.sort == "detail" && b.sortDown);
    CHECK(b.selected == s.selected && b.shown == s.shown && b.zoom == 8 && b.view == "wipe");
    CHECK(b.x == 10 && b.y == 20 && b.w == 1600 && b.h == 900 && b.paused);
    CHECK_EQ(b.chain, std::string("anime x4 tone grain=0.30"));  // Make again's last choice
    const studio::Session d = studio::sessionOf(json::Value::object());  // an empty one: the defaults
    CHECK(d.tab == "textures" && d.zoom == 1 && d.view == "pair" && d.x == -1 && !d.paused);
    CHECK(studio::sessionOf(json::Value("broken")).tab == "textures");
}

TEST(queue_runs_in_order) {
    studio::Queue q;
    const int64_t a = q.add("make", json::Value::object(), false);
    const int64_t b = q.add("make", json::Value::object(), false);
    CHECK(a == 1 && b == 2);
    studio::Job* j = q.next(true);
    CHECK(j && j->id == a);
    q.started(a);
    CHECK(q.next(true) && q.next(true)->id == b);  // the next queued one
    q.done(a);
    q.started(b);
    q.failed(b, "no network");
    CHECK(q.jobs()[1].state == studio::Job::Failed && q.jobs()[1].why == "no network");
    CHECK(!q.next(true));
}

TEST(queue_waits_while_the_pack_is_busy) {
    // Review Focus 1: a game writing the pack ([ai] generate) or a builder - a job that writes it waits
    studio::Queue q;
    const int64_t keep = q.add("keep", json::Value::object(), true);
    const int64_t make = q.add("make", json::Value::object(), false);
    CHECK(q.next(false) && q.next(false)->id == make);
    CHECK(q.next(true) && q.next(true)->id == keep);
}

TEST(queue_retries_once_after_the_engine_is_lost) {
    // Review Focus 4: the engine gone (a crash, an update) - once more on a new one, then failed
    studio::Queue q;
    const int64_t a = q.add("make", json::Value::object(), false);
    q.started(a);
    q.engineLost(a);
    CHECK(q.jobs()[0].state == studio::Job::Queued);
    CHECK(q.next(true) && q.next(true)->id == a);
    q.started(a);
    q.engineLost(a);
    CHECK(q.jobs()[0].state == studio::Job::Failed && !q.jobs()[0].why.empty());
    CHECK(!q.next(true));
}

TEST(queue_saves_running_as_queued) {
    // G4: Pause writes the queue; a job cut off runs again after Resume, finished ones are not kept
    studio::Queue q;
    json::Value args = json::Value::object();
    args.set("key", "256x256\\0123456789abcdef");
    const int64_t a = q.add("make", args, false);
    const int64_t b = q.add("keep", json::Value::object(), true);
    q.add("make", json::Value::object(), false);
    q.started(a);
    q.done(a);
    q.started(b);
    json::Value v;
    std::string error;
    CHECK(json::parse(json::dump(q.toJson()), v, error));
    studio::Queue back = studio::Queue::fromJson(v);
    CHECK_EQ(back.jobs().size(), size_t(2));
    CHECK(back.jobs()[0].id == b && back.jobs()[0].state == studio::Job::Queued && back.jobs()[0].writesPack);
    CHECK(back.add("make", json::Value::object(), false) == 4);  // ids go on
    CHECK(studio::Queue::fromJson(json::Value()).jobs().empty());
}

TEST(engine_name_follows_the_build) {
    // G3: a copy of the installed uaro-render64.exe a build of the driver
    const std::string a = studio::engineName("2.2.5 build 2026-10-18 12:34", 4000000, 1760000000);
    CHECK(a.compare(0, 13, "engine-2.2.5-") == 0 && a.size() == 25 && a.substr(21) == ".exe");
    CHECK(a == studio::engineName("2.2.5 build 2026-10-18 12:34", 4000000, 1760000000));
    CHECK(a != studio::engineName("2.2.5 build 2026-10-18 12:35", 4000000, 1760000000));
    CHECK(a != studio::engineName("2.2.5 build 2026-10-18 12:34", 4000001, 1760000000));
    CHECK(studio::engineName("..\\x y", 1, 1).compare(0, 9, "engine-x-") == 0);  // nothing of a path
}

TEST(builds_drop_all_but_five_and_the_running) {
    std::vector<std::pair<std::string, int64_t>> builds;
    for (int i = 0; i < 8; ++i) builds.emplace_back("b" + std::to_string(i), 100 + i);
    std::vector<std::string> gone = studio::buildsToDrop(builds, "b7");
    CHECK(gone == (std::vector<std::string>{"b0", "b1", "b2"}));
    gone = studio::buildsToDrop(builds, "b1");  // an old one still running stays
    CHECK(gone == (std::vector<std::string>{"b0", "b2"}));
    CHECK(studio::buildsToDrop({{"a", 1}}, "a").empty());
}

TEST(queue_pause_is_no_try) {
    // G4: Pause stops the engine in the middle of a job - it runs again whole after Resume, not counted as a failure
    studio::Queue q;
    const int64_t a = q.add("make", json::Value::object(), false);
    q.started(a);
    q.interrupted(a);
    CHECK(q.jobs()[0].state == studio::Job::Queued && q.jobs()[0].tries == 0);
    q.started(a);
    q.engineLost(a);
    CHECK(q.jobs()[0].state == studio::Job::Queued);  // still its one more try
}
