// 2.12: the safe mode - a game that failed to start twice in a row starts on Direct3D 11 without the risky parts.
#include <string>

#include "core/renderer_choice.hpp"
#include "core/safe_mode.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(safe_mode_after_two_failed_starts) {
    CHECK_EQ(safe::failedInRow(0, safe::Start::Failed), 1);
    CHECK_EQ(safe::failedInRow(1, safe::Start::Failed), 2);
    CHECK_EQ(safe::failedInRow(2, safe::Start::Fine), 0);  // a good start: counted from nothing again
    CHECK(!safe::needed(1, false));
    CHECK(safe::needed(2, false));
    CHECK(safe::needed(0, true));  // [renderer] safe_mode = 1
}

TEST(a_crash_after_the_first_picture_is_no_failed_start) {
    const std::string start = "uaRO Graphics 2.12.0 in C:\\uaRO\\uaRO.exe\n(uaRO renderer)\n";
    const std::string crash = "crash: exception 0xc0000005 at DDRAW.dll+0x1234\n";
    CHECK(safe::startFailed(start + crash));                                       // before any picture
    CHECK(!safe::startFailed(start + std::string(safe::kFirstPicture) + "\n" + crash));  // two hours later
    CHECK(safe::startFailed(start + "the uaRO renderer could not start\n"));
    CHECK(!safe::startFailed(start + std::string(safe::kFirstPicture) + "\nDirectDraw released, renderer closed\n"));
    CHECK(!safe::startFailed(""));  // no game before
}

TEST(safe_config_turns_the_risky_parts_off) {
    Config c;
    c.api = Api::D3d12;
    c.bridge = c.remix = c.hdr = c.renderThread = true;
    c.dlss = "dlaa";
    c.frameGeneration = 2;
    c.ai.enabled = true;
    c.ai.generate = true;
    safe::apply(c);
    CHECK(c.api == Api::D3d11);
    CHECK(!c.bridge && !c.remix && !c.hdr && !c.renderThread);
    CHECK(c.dlss == "off" && c.frameGeneration == 0);
    CHECK(!c.ai.on());
    CHECK(c.effects == effectsOff());
    Config off;
    off.api = Api::System;  // switched off stays off
    safe::apply(off);
    CHECK(off.api == Api::System);
}

TEST(safe_mode_without_direct3d_11_is_the_games_own_look) {
    Config c;
    safe::apply(c);
    SystemFacts f;
    f.windows10 = f.d3d12 = true;  // Direct3D 12 there, 11 not: still no risk taken
    CHECK(chooseRenderer(c, f, true).renderer == Renderer::System);
}

// A start sent to «Гра» (or with uaRO Graphics switched off) draws nothing of ours: it neither fails nor goes well -
// the count of failed starts stays as it was (a player who starts from the old shortcut every time).
TEST(a_start_that_drew_nothing_leaves_the_count) {
    const std::string redirected =
        "uaRO Graphics 2.12.0 in C:\\uaRO\\uaRO.exe\nlauncher: a direct start - «Гра» opens\n";
    CHECK(safe::judge(redirected) == safe::Start::Unknown);
    CHECK_EQ(safe::failedInRow(1, safe::judge(redirected)), 1);
    CHECK(safe::judge("(uaRO renderer)\ncrash: exception 0xc0000005\n") == safe::Start::Failed);
    CHECK(safe::judge(std::string("(uaRO renderer)\n") + safe::kFirstPicture + "\n") == safe::Start::Fine);
    CHECK_EQ(safe::failedInRow(1, safe::Start::Failed), 2);
    CHECK_EQ(safe::failedInRow(1, safe::Start::Fine), 0);
}

// 3.0.11-beta.59 (the autopilot): a game that ended badly within 5 minutes of its first picture counts too - a crash at
// the login or at a map's loading repeats at every start; a later one, or a log without times, as before
TEST(safe_mode_a_crash_in_the_first_minutes_counts) {
    const std::string start = "09:59:58.000 INFO  uaRO Graphics 3.0.11 in C:\\uaRO\\uaRO.exe\n"
                              "09:59:59.000 INFO  (uaRO renderer)\n";
    const std::string pic = std::string("10:00:00.100 INFO  ") + safe::kFirstPicture + "\n";
    const std::string crash = " ERROR crash: exception 0xc0000005 at DDRAW.dll+0x1234\n";
    CHECK(safe::judge(start + pic + "10:01:30.000" + crash) == safe::Start::FailedEarly);
    CHECK(safe::judge(start + pic + "10:06:00.000" + crash) == safe::Start::Fine);
    CHECK(safe::judge(start + pic + "10:02:00.000 INFO  map: prontera\n") == safe::Start::FailedEarly);  // not closed
    CHECK(safe::judge(start + pic + "10:02:00.000 INFO  map: prontera\n10:30:00.000 INFO  DirectDraw released, "
                                    "renderer closed\n") == safe::Start::Fine);
    CHECK(safe::judge(start + pic + "10:02:00.000 ERROR the GPU device was removed (0x887a0006)\n") ==
          safe::Start::FailedEarly);
    const std::string late =
        "23:58:58.000 INFO  (uaRO renderer)\n23:59:00.000 INFO  " + std::string(safe::kFirstPicture) + "\n";
    CHECK(safe::judge(late + "00:01:00.000" + crash) == safe::Start::FailedEarly);  // past midnight
    CHECK_EQ(safe::failedInRow(1, safe::Start::FailedEarly), 2);
    CHECK(safe::startFailed(start + pic + "10:01:30.000" + crash));
}

// 3.0.11-beta.106 (R-N6M9DY: a game closed a minute after its start crashed in the system's DINPUT.dll 1 ms after
// «DirectDraw released, renderer closed» - counted as a failed start; two such short games and the safe mode came):
// a crash after the renderer's last opening was closed is the game's own exit - the start went fine
TEST(safe_mode_a_crash_after_the_renderer_closed_is_the_games_exit) {
    const std::string start = "16:26:09.000 INFO  uaRO Graphics 3.0.11 in C:\\uaRO\\uaRO.exe\n"
                              "16:26:09.500 INFO  (uaRO renderer)\n";
    const std::string pic = std::string("16:26:10.100 INFO  ") + safe::kFirstPicture + "\n";
    const std::string closed = "16:27:12.815 INFO  DirectDraw released, renderer closed\n";
    const std::string crash = "16:27:12.816 ERROR crash: exception 0xc0000005 at DINPUT.dll+0x8ade (thread 23816)\n";
    CHECK(safe::judge(start + pic + closed + crash) == safe::Start::Fine);
    CHECK(!safe::startFailed(start + pic + closed + crash));
    // the game opened DirectDraw again (a mode change) and crashed in it: a failed start still
    const std::string again = "16:27:13.000 INFO  (uaRO renderer)\n";
    CHECK(safe::judge(start + pic + closed + again + crash) == safe::Start::FailedEarly);
    // a crash before the close: as before
    CHECK(safe::judge(start + pic + crash + closed) == safe::Start::FailedEarly);
}

// 3.0.11-beta.59: the automatic safe mode's note says which version wrote it - another version tries the normal mode
TEST(safe_mode_note_carries_its_version) {
    const std::string note = safe::noteText(2, "3.0.11-beta.59");
    CHECK(note.rfind("version 3.0.11-beta.59\n", 0) == 0);
    CHECK_EQ(safe::noteVersion(note), std::string("3.0.11-beta.59"));
    CHECK_EQ(safe::noteVersion("2 starts in a row ended before their first picture"), std::string(""));  // 2.12's
    CHECK_EQ(safe::noteVersion(""), std::string(""));
}
